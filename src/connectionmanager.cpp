/*
 * Copyright (c) 2011, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "shellapi.h"
#include "config.h"
#include "logging.h"
#include "psiclient.h"
#include "connectionmanager.h"
#include "server_request.h"
#include "httpsrequest.h"
#include "webbrowser.h"
#include "embeddedvalues.h"
#include "usersettings.h"
#include "zlib.h"
#include <algorithm>
#include <sstream>
#include <Shlwapi.h>
#include "transport.h"
#include "transport_registry.h"
#include "transport_connection.h"
#include "authenticated_data_package.h"
#include "stopsignal.h"
#include "diagnostic_info.h"


// Upgrade process posts a Quit message
extern HWND g_hWnd;


ConnectionManager::ConnectionManager(void) :
    m_state(CONNECTION_MANAGER_STATE_STOPPED),
    m_thread(0),
    m_upgradeThread(0),
    m_feedbackThread(0),
    m_transport(0),
    m_upgradePending(false),
    m_startSplitTunnel(false),
    m_nextFetchRemoteServerListAttempt(0)
{
    m_mutex = CreateMutex(NULL, FALSE, 0);

    Settings::Initialize();
}

ConnectionManager::~ConnectionManager(void)
{
    Stop(STOP_REASON_NONE);
    CloseHandle(m_mutex);
}

void ConnectionManager::OpenHomePages(const TCHAR* defaultHomePage/*=0*/, bool allowSkip/*=true*/)
{
    AutoMUTEX lock(m_mutex);

    if (!allowSkip || !Settings::SkipBrowser())
    {
        vector<tstring> urls = m_currentSessionInfo.GetHomepages();
        if (urls.size() == 0 && defaultHomePage)
        {
            urls.push_back(defaultHomePage);
        }
        OpenBrowser(urls);
    }
}

void ConnectionManager::SetState(ConnectionManagerState newState)
{
    // NOTE: no lock, to prevent blocking connection thread with UI polling
    // Starting Time is informational only, consistency with state isn't critical

    m_state = newState;

    if (newState == CONNECTION_MANAGER_STATE_STARTING)
    {
        UI_SetStateStarting(m_transport->GetTransportProtocolName());
    }
    else if (newState == CONNECTION_MANAGER_STATE_CONNECTED)
    {
        UI_SetStateConnected(
            m_transport->GetTransportProtocolName(),
            m_currentSessionInfo.GetLocalSocksProxyPort(),
            m_currentSessionInfo.GetLocalHttpProxyPort());
    }
    else if (newState == CONNECTION_MANAGER_STATE_STOPPING)
    {
        UI_SetStateStopping();
    }
    else //if (newState == CONNECTION_MANAGER_STATE_STOPPED)
    {
        UI_SetStateStopped();
    }
}

ConnectionManagerState ConnectionManager::GetState()
{
    return m_state;
}

// Note: these IReconnectStateReceiver functions allow e.g., a Transport, to
// indirectly update the connection state. In the core case, this is used
// when the core process automatically reconnects when all tunnels are
// disconnected -- without ending the Transport lifetime.
// These m_state changes are currently safe to make... unlike changing
// m_state to CONNECTION_MANAGER_STATE_STOPPED, which would break Toggle.
//

void ConnectionManager::SetReconnecting()
{
    SetState(CONNECTION_MANAGER_STATE_STARTING);
}

void ConnectionManager::SetReconnected()
{
    SetState(CONNECTION_MANAGER_STATE_CONNECTED);
}

void ConnectionManager::Toggle()
{
    // NOTE: no lock, to allow thread to access object

    if (m_state == CONNECTION_MANAGER_STATE_STOPPED)
    {
        Start();
    }
    else
    {
        Stop(STOP_REASON_USER_DISCONNECT);
    }
}

void ConnectionManager::Stop(DWORD reason)
{
    my_print(NOT_SENSITIVE, true, _T("%s: enter"), __TFUNCTION__);

    // NOTE: no lock, to allow thread to access object

    // The assumption is that signalling stop will cause any current operations to
    // stop (such as making HTTPS requests, or establishing a connection), and
    // cause the connection to hang up if it is connected.
    // While a connection is active, there is a thread running waiting for the
    // connection to terminate.

    // This will signal (some) running tasks to terminate.
    GlobalStopSignal::Instance().SignalStop(reason);

    SetState(CONNECTION_MANAGER_STATE_STOPPING);

    // Wait for thread to exit (otherwise can get access violation when app terminates)
    if (m_thread)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Waiting for thread to die"), __TFUNCTION__);
        WaitForSingleObject(m_thread, INFINITE);
        my_print(NOT_SENSITIVE, true, _T("%s: Thread died"), __TFUNCTION__);
        m_thread = 0;
    }

    if (m_upgradeThread)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Waiting for upgrade thread to die"), __TFUNCTION__);
        WaitForSingleObject(m_upgradeThread, INFINITE);
        my_print(NOT_SENSITIVE, true, _T("%s: Upgrade thread died"), __TFUNCTION__);
        m_upgradeThread = 0;
    }

    if (m_feedbackThread)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Waiting for feedback thread to die"), __TFUNCTION__);
        WaitForSingleObject(m_feedbackThread, INFINITE);
        my_print(NOT_SENSITIVE, true, _T("%s: Feedback thread died"), __TFUNCTION__);
        m_feedbackThread = 0;
    }

    delete m_transport;
    m_transport = 0;

    SetState(CONNECTION_MANAGER_STATE_STOPPED);

    my_print(NOT_SENSITIVE, true, _T("%s: exit"), __TFUNCTION__);
}

void ConnectionManager::Start()
{
    my_print(NOT_SENSITIVE, true, _T("%s: enter"), __TFUNCTION__);

    // Call Stop to cleanup in case thread failed on last Start attempt
    Stop(STOP_REASON_USER_DISCONNECT);

    AutoMUTEX lock(m_mutex);

    m_transport = TransportRegistry::New(Settings::Transport());

    m_startSplitTunnel = Settings::SplitTunnel();

    GlobalStopSignal::Instance().ClearStopSignal(STOP_REASON_ALL &~ STOP_REASON_EXIT);

    if (m_state != CONNECTION_MANAGER_STATE_STOPPED || m_thread != 0)
    {
        my_print(NOT_SENSITIVE, false, _T("Invalid connection manager state in Start (%d)"), m_state);
        return;
    }

    SetState(CONNECTION_MANAGER_STATE_STARTING);

    if (!(m_thread = CreateThread(0, 0, ConnectionManagerStartThread, (void*)this, 0, 0)))
    {
        my_print(NOT_SENSITIVE, false, _T("Start: CreateThread failed (%d)"), GetLastError());

        SetState(CONNECTION_MANAGER_STATE_STOPPED);
    }

    my_print(NOT_SENSITIVE, true, _T("%s: exit"), __TFUNCTION__);
}

DWORD WINAPI ConnectionManager::ConnectionManagerStartThread(void* object)
{
    my_print(NOT_SENSITIVE, true, _T("%s: enter"), __TFUNCTION__);

    ConnectionManager* manager = (ConnectionManager*)object;

    // Seed built-in non-crypto PRNG used for shuffling (load balancing)
    unsigned int seed = (unsigned)time(NULL);
    srand(seed);

    // We only want to open the home page once per retry loop.
    // This prevents auto-reconnect from opening the home page again.
    bool homePageOpened = false;

    // Keep track of whether we've already hit a NoServers exception.
    bool noServers = false;

    //
    // Repeatedly attempt to connect.
    //
    // All operations may be interrupted by user cancel.
    //
    // NOTE: this function doesn't hold the ConnectionManager
    // object lock to allow for cancel etc.

    while (true)
    {
        if (manager->m_upgradePending)
        {
            // An upgrade has been downloaded and paved.  Since there is no
            // currently connected tunnel, go ahead and restart the application
            // using the new version.
            // TODO: if ShellExecute fails, don't die?
            TCHAR filename[1000];
            if (GetModuleFileName(NULL, filename, 1000))
            {
                ShellExecute(0, NULL, filename, 0, 0, SW_SHOWNORMAL);
                PostMessage(g_hWnd, WM_QUIT, 0, 0);
                break;
            }
        }

        my_print(NOT_SENSITIVE, true, _T("%s: enter server loop"), __TFUNCTION__);

        // Timer measures tunnel lifetime
        DWORD tunnelStartTime = 0;

        try
        {
            GlobalStopSignal::Instance().CheckSignal(STOP_REASON_ALL, true);

            manager->SetState(CONNECTION_MANAGER_STATE_STARTING);

            // Do we have any usable servers?
            if (!manager->m_transport->ServerWithCapabilitiesExists())
            {
                my_print(NOT_SENSITIVE, false, _T("No known servers support this transport"), __TFUNCTION__);
                throw TransportConnection::NoServers();
            }

            //
            // Set up the transport connection
            //

            my_print(NOT_SENSITIVE, true, _T("%s: doing transportConnection for %s"), __TFUNCTION__, manager->m_transport->GetTransportDisplayName().c_str());

            // Note that the TransportConnection will do any necessary cleanup.
            TransportConnection transportConnection;

            // May throw TryNextServer
            try
            {
                transportConnection.Connect(
                    StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ALL),
                    manager->m_transport,
                    manager,    // ILocalProxyStatsCollector
                    manager);   // IReconnectStateReceiver
            }
            catch (TransportConnection::TryNextServer&)
            {
                throw;
            }

            tunnelStartTime = GetTickCount();

            //
            // The transport connection did a handshake, so its sessionInfo is
            // fuller than ours. Update ours and then update the server entries.
            //

            SessionInfo sessionInfo = transportConnection.GetUpdatedSessionInfo();
            manager->UpdateCurrentSessionInfo(sessionInfo);

            //
            // If handshake notified of new version, start the upgrade in a (background) thread
            //

            if (manager->RequireUpgrade())
            {
                if (!manager->m_upgradeThread ||
                    WAIT_OBJECT_0 == WaitForSingleObject(manager->m_upgradeThread, 0))
                {
                    if (!(manager->m_upgradeThread = CreateThread(0, 0, ConnectionManagerUpgradeThread, manager, 0, 0)))
                    {
                        my_print(NOT_SENSITIVE, false, _T("Upgrade: CreateThread failed (%d)"), GetLastError());
                    }
                }
            }

            // Before doing post-connect work, make sure there's no stop signal.
            // Throws if there's a signal set.
            GlobalStopSignal::Instance().CheckSignal(STOP_REASON_ALL, true);

            //
            // Do post-connect work, like opening home pages.
            //

            my_print(NOT_SENSITIVE, true, _T("%s: transport succeeded; DoPostConnect"), __TFUNCTION__);
            manager->DoPostConnect(sessionInfo, !homePageOpened);
            homePageOpened = true;

            //
            // Wait for transportConnection to stop (or fail)
            //

            my_print(NOT_SENSITIVE, true, _T("%s: entering transportConnection wait"), __TFUNCTION__);
            transportConnection.WaitForDisconnect();

            GlobalStopSignal::Instance().CheckSignal(STOP_REASON_ALL, true);

            // The stop signal has not been set, so this was an unexpected disconnect. Retry.

            throw TransportConnection::TryNextServer();
        }
        catch (TransportConnection::TryNextServer&)
        {
            my_print(NOT_SENSITIVE, true, _T("%s: caught TryNextServer"), __TFUNCTION__);
            // Fall through
        }
        catch (TransportConnection::PermanentFailure&)
        {
            // Unrecoverable error. Cleanup and exit.
            my_print(NOT_SENSITIVE, true, _T("%s: caught TransportConnection::PermanentFailure"), __TFUNCTION__);
            manager->SetState(CONNECTION_MANAGER_STATE_STOPPED);
            break;
        }
        catch (TransportConnection::NoServers&)
        {
            my_print(NOT_SENSITIVE, true, _T("%s: caught NoServers"), __TFUNCTION__);
            // On the first NoServers we fall through so that we can FetchRemoteServerList.
            // On the second NoServers we bail out.
            if (noServers)
            {
                manager->SetState(CONNECTION_MANAGER_STATE_STOPPED);
                break;
            }
            // else fall through
            noServers = true;
        }
        catch (StopSignal::UnexpectedDisconnectStopException& ex)
        {
            my_print(NOT_SENSITIVE, true, _T("%s: caught StopSignal::UnexpectedDisconnectStopException"), __TFUNCTION__);
            GlobalStopSignal::Instance().ClearStopSignal(ex.GetType());
            // Fall through
        }
        catch (IWorkerThread::Error& error)
        {
            // Unrecoverable error. Cleanup and exit.
            my_print(NOT_SENSITIVE, true, _T("%s: caught ITransport::Error: %s"), __TFUNCTION__, error.GetMessage().c_str());
            manager->SetState(CONNECTION_MANAGER_STATE_STOPPED);
            break;
        }
        catch (IWorkerThread::Abort&)
        {
            // User requested cancel. Cleanup and exit.
            my_print(NOT_SENSITIVE, true, _T("%s: caught IWorkerThread::Abort"), __TFUNCTION__);
            manager->SetState(CONNECTION_MANAGER_STATE_STOPPED);
            break;
        }
        // Catch the StopException base class
        catch (StopSignal::StopException&)
        {
            // User requested cancel or transport died, etc. Cleanup and exit.
            my_print(NOT_SENSITIVE, true, _T("%s: caught StopSignal::StopException"), __TFUNCTION__);
            manager->SetState(CONNECTION_MANAGER_STATE_STOPPED);
            break;
        }
        catch (ConnectionManager::Abort&)
        {
            my_print(NOT_SENSITIVE, true, _T("%s: caught ConnectionManager::Abort"), __TFUNCTION__);
            manager->SetState(CONNECTION_MANAGER_STATE_STOPPED);
            break;
        }

        // Failed to connect to the server. Try the next one.

        manager->SetState(CONNECTION_MANAGER_STATE_STARTING);

        // Give users some feedback. Before, when the handshake failed
        // all we displayed was "WinHttpCallbackFailed (200000)" and kept
        // the arrow animation spinning. A user-authored FAQ mentioned
        // this error in particular and recommended waiting. So here's
        // a lightly more encouraging message.
        my_print(NOT_SENSITIVE, false, _T("Still trying..."));

        // Continue while loop to try next server

        manager->FetchRemoteServerList();

        // Wait between 1 and 2 seconds before retrying. This is a quick
        // fix to deal with the following problem: when a client can
        // make an HTTPS connection but not a VPN connection, it ends
        // up spamming "handshake" requests, resulting in PSK race conditions
        // with other clients that are trying to connect. This is starving
        // clients that are able to establish the VPN connection.
        // TODO: a more optimal solution would only wait when re-trying
        // a server where this condition (HTTPS ok, VPN failed) previously
        // occurred.
        // UPDATE: even with SSH as a fail over, we're leaving this delay
        // in for now as clients blocked on both protocols would otherwise
        // still spam handshakes. The delay is *after* SSH fail over so as
        // not to delay that attempt (on the same server).
        Sleep(1000 + rand()%1000);
    }

    my_print(NOT_SENSITIVE, true, _T("%s: exiting thread"), __TFUNCTION__);
    return 0;
}

void ConnectionManager::DoPostConnect(const SessionInfo& sessionInfo, bool openHomePages)
{
    // Called from connection thread
    // NOTE: no lock while waiting for network events

    SetState(CONNECTION_MANAGER_STATE_CONNECTED);

    if (m_transport->RequiresStatsSupport())
    {
        //
        // "Connected" HTTPS request for server stats.
        // It's not critical if this request fails so failure is ignored.
        //

        tstring connectedRequestPath = GetConnectRequestPath(m_transport);

        DWORD start = GetTickCount();
        string response;
        if (ServerRequest::MakeRequest(
                            ServerRequest::ONLY_IF_TRANSPORT,
                            m_transport,
                            sessionInfo,
                            connectedRequestPath.c_str(),
                            response,
                            StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ALL)))
        {
            // Get the server time from response json and record it
            // connected_timestamp
            Json::Value json_entry;
            Json::Reader reader;
            string connected_timestamp;

            bool parsingSuccessful = reader.parse(response, json_entry);
            if(parsingSuccessful)
            {
                try
                {
                    connected_timestamp = json_entry.get("connected_timestamp", 0).asString();
                    RegistryFailureReason reason = REGISTRY_FAILURE_NO_REASON;
                    (void)WriteRegistryStringValue(
                            LOCAL_SETTINGS_REGISTRY_VALUE_LAST_CONNECTED,
                            connected_timestamp,
                            reason);
                }
                catch (exception& e)
                {
                    my_print(NOT_SENSITIVE, false, _T("%s: JSON parse exception: %S"), __TFUNCTION__, e.what());
                }
            }
            else
            {
                string fail = reader.getFormattedErrorMessages();
                my_print(NOT_SENSITIVE, false, _T("%s:%d: 'connected' response parse failed: %S"), __TFUNCTION__, __LINE__, reader.getFormattedErrorMessages().c_str());
            }
        }
    }

    //
    // Open home pages in browser
    //

    if (openHomePages)
    {
        OpenHomePages();
    }
}

bool ConnectionManager::SendStatusMessage(
                            bool final,
                            const map<string, int>& pageViewEntries,
                            const map<string, int>& httpsRequestEntries,
                            unsigned long long bytesTransferred)
{
    // NOTE: no lock while waiting for network events

    // Make a copy of SessionInfo for threadsafety.
    SessionInfo sessionInfo;
    {
        AutoMUTEX lock(m_mutex);
        sessionInfo = m_currentSessionInfo;
    }

    // Format stats data for consumption by the server.

    Json::Value stats;

    // Stats traffic analysis mitigation: [non-cryptographic] pseudorandom padding to ensure the size of status requests is not constant.
    // Padding size is JSON field overhead + 0-255 bytes + 33% base64 encoding overhead
    const int MAX_PADDING_LENGTH = 256;
    unsigned char pseudorandom_bytes[MAX_PADDING_LENGTH];
    assert(MAX_PADDING_LENGTH % sizeof(unsigned int) == 0);
    for (int i = 0; i < MAX_PADDING_LENGTH/sizeof(unsigned int); i++)
    {
        rand_s(((unsigned int*)pseudorandom_bytes) + i);
    }
    string padding = Base64Encode(pseudorandom_bytes, rand() % 256);
    stats["padding"] = padding;

    stats["bytes_transferred"] = bytesTransferred;
    my_print(SENSITIVE_LOG, true, _T("BYTES: %llu"), bytesTransferred);

    map<string, int>::const_iterator pos = pageViewEntries.begin();
    Json::Value page_views(Json::arrayValue);
    for (; pos != pageViewEntries.end(); pos++)
    {
        Json::Value entry;
        entry["page"] = pos->first;
        entry["count"] = pos->second;
        page_views.append(entry);
        my_print(SENSITIVE_LOG, true, _T("PAGEVIEW: %d: %S"), pos->second, pos->first.c_str());
    }
    stats["page_views"] = page_views;

    pos = httpsRequestEntries.begin();
    Json::Value https_requests(Json::arrayValue);
    for (; pos != httpsRequestEntries.end(); pos++)
    {
        Json::Value entry;
        entry["domain"] = pos->first;
        entry["count"] = pos->second;
        https_requests.append(entry);
        my_print(SENSITIVE_LOG, true, _T("HTTPS REQUEST: %d: %S"), pos->second, pos->first.c_str());
    }
    stats["https_requests"] = https_requests;

    ostringstream additionalData;
    Json::FastWriter jsonWriter;
    additionalData << jsonWriter.write(stats);
    string additionalDataString = additionalData.str();

    tstring requestPath = GetStatusRequestPath(m_transport, !final);
    if (requestPath.length() <= 0)
    {
        // Can't send the status
        return false;
    }

    string response;

    // When disconnected, ignore the user cancel flag in the HTTP request
    // wait loop.
    // TODO: the user may be left waiting too long after cancelling; add
    // a shorter timeout in this case
    DWORD stopReason = final ? STOP_REASON_NONE : STOP_REASON_ALL;

    // Allow adhoc tunnels if this is the final stats request
    ServerRequest::ReqLevel reqLevel = final ? ServerRequest::FULL : ServerRequest::ONLY_IF_TRANSPORT;

    bool success = ServerRequest::MakeRequest(
                                    reqLevel,
                                    m_transport,
                                    sessionInfo,
                                    requestPath.c_str(),
                                    response,
                                    StopInfo(&GlobalStopSignal::Instance(), stopReason),
                                    L"Content-Type: application/json",
                                    (LPVOID)additionalDataString.c_str(),
                                    additionalDataString.length());

    return success;
}

tstring ConnectionManager::GetFailedRequestPath(ITransport* transport)
{
    AutoMUTEX lock(m_mutex);

    return tstring(HTTP_FAILED_REQUEST_PATH) +
           _T("?client_session_id=") + UTF8ToWString(m_currentSessionInfo.GetClientSessionID()) +
           _T("&propagation_channel_id=") + UTF8ToWString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + UTF8ToWString(SPONSOR_ID) +
           _T("&client_version=") + UTF8ToWString(CLIENT_VERSION) +
           _T("&server_secret=") + UTF8ToWString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") +  transport->GetTransportRequestName() +
           _T("&error_code=") + transport->GetLastTransportError();
}

tstring ConnectionManager::GetConnectRequestPath(ITransport* transport)
{
    AutoMUTEX lock(m_mutex);

    // Get info about the previous connected event
    string lastConnected;
    // Don't check the return value -- use the default empty string if not found.
    (void)ReadRegistryStringValue(LOCAL_SETTINGS_REGISTRY_VALUE_LAST_CONNECTED, lastConnected);
    if (lastConnected.length() == 0) lastConnected = "None";

    return tstring(HTTP_CONNECTED_REQUEST_PATH) +
           _T("?client_session_id=") + UTF8ToWString(m_currentSessionInfo.GetClientSessionID()) +
           _T("&propagation_channel_id=") + UTF8ToWString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + UTF8ToWString(SPONSOR_ID) +
           _T("&client_version=") + UTF8ToWString(CLIENT_VERSION) +
           _T("&server_secret=") + UTF8ToWString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") + transport->GetTransportRequestName() +
           _T("&session_id=") + transport->GetSessionID(m_currentSessionInfo) +
           _T("&last_connected=") + UTF8ToWString(lastConnected);
}

tstring ConnectionManager::GetStatusRequestPath(ITransport* transport, bool connected)
{
    AutoMUTEX lock(m_mutex);

    tstring sessionID = transport->GetSessionID(m_currentSessionInfo);

    // If there's no session ID, we can't send the status.
    if (sessionID.length() <= 0)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: no session ID; not sending status"), __TFUNCTION__);
        return _T("");
    }

    // TODO: get error code from SSH client?

    return tstring(HTTP_STATUS_REQUEST_PATH) +
           _T("?client_session_id=") + UTF8ToWString(m_currentSessionInfo.GetClientSessionID()) +
           _T("&propagation_channel_id=") + UTF8ToWString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + UTF8ToWString(SPONSOR_ID) +
           _T("&client_version=") + UTF8ToWString(CLIENT_VERSION) +
           _T("&server_secret=") + UTF8ToWString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") +  transport->GetTransportRequestName() +
           _T("&session_id=") + sessionID +
           _T("&connected=") + (connected ? _T("1") : _T("0"));
}

void ConnectionManager::GetUpgradeRequestInfo(SessionInfo& sessionInfo, tstring& requestPath)
{
    AutoMUTEX lock(m_mutex);

    sessionInfo = m_currentSessionInfo;
    requestPath = tstring(HTTP_DOWNLOAD_REQUEST_PATH) +
                    _T("?client_session_id=") + UTF8ToWString(m_currentSessionInfo.GetClientSessionID()) +
                    _T("&propagation_channel_id=") + UTF8ToWString(PROPAGATION_CHANNEL_ID) +
                    _T("&sponsor_id=") + UTF8ToWString(SPONSOR_ID) +
                    _T("&client_version=") + UTF8ToWString(m_currentSessionInfo.GetUpgradeVersion()) +
                    _T("&server_secret=") + UTF8ToWString(m_currentSessionInfo.GetWebServerSecret());
}


// ==== General Session Functions =============================================

void ConnectionManager::FetchRemoteServerList()
{
    // Note: not used by CoreTransport

    AutoMUTEX lock(m_mutex);

    if (strlen(REMOTE_SERVER_LIST_ADDRESS) == 0)
    {
        return;
    }

    // After at least one failed connection attempt, and no more than once
    // per few hours (if successful), or not more than once per few minutes
    // (if unsuccessful), check for a new remote server list.
    if (m_nextFetchRemoteServerListAttempt != 0 &&
        m_nextFetchRemoteServerListAttempt > time(0))
    {
        return;
    }

    m_nextFetchRemoteServerListAttempt = time(0) + SECONDS_BETWEEN_UNSUCCESSFUL_REMOTE_SERVER_LIST_FETCH;

    string response;

    try
    {
        HTTPSRequest httpsRequest;
        // NOTE: Not using local proxy
        if (!httpsRequest.MakeRequest(
                UTF8ToWString(REMOTE_SERVER_LIST_ADDRESS).c_str(),
                443,
                "",
                UTF8ToWString(REMOTE_SERVER_LIST_REQUEST_PATH).c_str(),
                response,
                StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_EXIT),
                false, // don't use local proxy
                true)  // fail over to URL proxy
            || response.length() <= 0)
        {
            my_print(NOT_SENSITIVE, false, _T("Fetch remote server list failed"));
            return;
        }
    }
    catch (StopSignal::StopException&)
    {
        // Application is exiting.
        return;
    }

    m_nextFetchRemoteServerListAttempt = time(0) + SECONDS_BETWEEN_SUCCESSFUL_REMOTE_SERVER_LIST_FETCH;

    string serverEntryList;
    if (!verifySignedDataPackage(
            REMOTE_SERVER_LIST_SIGNATURE_PUBLIC_KEY,
            response.c_str(),
            response.length(),
            false, // zipped, not gzipped
            serverEntryList))
    {
        my_print(NOT_SENSITIVE, false, _T("Verify remote server list failed"));
        return;
    }

    vector<string> newServerEntryVector;
    istringstream serverEntryListStream(serverEntryList);
    string line;
    while (getline(serverEntryListStream, line))
    {
        if (!line.empty())
        {
            newServerEntryVector.push_back(line);
        }
    }

    try
    {
        // This adds the new server entries to all transports' server lists.
        TransportRegistry::AddServerEntries(newServerEntryVector, 0);

        my_print(NOT_SENSITIVE, true, _T("%s: %d server entries"), __TFUNCTION__, newServerEntryVector.size());
    }
    catch (std::exception &ex)
    {
        my_print(NOT_SENSITIVE, false, string("Corrupt remote server list: ") + ex.what());
        // This isn't fatal.
    }
}

bool ConnectionManager::RequireUpgrade(void)
{
    AutoMUTEX lock(m_mutex);

    return !m_upgradePending && m_currentSessionInfo.GetUpgradeVersion().size() > 0;
}

DWORD WINAPI ConnectionManager::ConnectionManagerUpgradeThread(void* object)
{
    my_print(NOT_SENSITIVE, true, _T("%s: enter"), __TFUNCTION__);

    my_print(NOT_SENSITIVE, false, _T("Downloading new version..."));

    ConnectionManager* manager = (ConnectionManager*)object;

    try
    {
        SessionInfo sessionInfo;
        tstring downloadRequestPath;
        string downloadResponse;
        // Note that this is getting the current session info, which is set
        // by LoadNextServer.  So it's unlikely but possible that we may be
        // loading the next server after the first one that notified us of an
        // upgrade failed to connect.  This still should not be a problem, since
        // all servers should have the same upgrades available.
        manager->GetUpgradeRequestInfo(sessionInfo, downloadRequestPath);

        // Download new binary
        DWORD start = GetTickCount();
        HTTPSRequest httpsRequest;
        if (!httpsRequest.MakeRequest(
                UTF8ToWString(UPGRADE_ADDRESS).c_str(),
                443,
                "",
                UTF8ToWString(UPGRADE_REQUEST_PATH).c_str(),
                downloadResponse,
                StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ALL),
                true, // tunnel request
                true) // fail over to URL proxy
            || downloadResponse.length() <= 0)
        {
            // If the download failed, we simply do nothing.
            // Rationale:
            // - The server is (and hopefully will remain) backwards compatible.
            // - The failure is likely a configuration one, as the handshake worked.
            // - A configuration failure could be common across all servers, so the
            //   client will never connect.
            // - Fail-over exposes new server IPs to hostile networks, so we don't
            //   like doing it in the case where we know the handshake already succeeded.
        }
        else
        {
            my_print(NOT_SENSITIVE, false, _T("Download complete"));

            // Perform upgrade.

            string upgradeData;

            if (verifySignedDataPackage(
                    UPGRADE_SIGNATURE_PUBLIC_KEY,
                    downloadResponse.c_str(),
                    downloadResponse.length(),
                    true, // gzip compressed
                    upgradeData))
            {
                // Data in the package is Base64 encoded
                upgradeData = Base64Decode(upgradeData);

                if (upgradeData.length() > 0)
                {
                    manager->PaveUpgrade(upgradeData);
                }
            }
            else
            {
                // Bad package. Log and continue.
                my_print(NOT_SENSITIVE, false, _T("Upgrade package verification failed! Please report this error."));
            }
        }
    }
    catch (StopSignal::StopException&)
    {
        // do nothing, just exit
    }

    my_print(NOT_SENSITIVE, true, _T("%s: exiting thread"), __TFUNCTION__);
    return 0;
}

void ConnectionManager::PaveUpgrade(const string& download)
{
    AutoMUTEX lock(m_mutex);

    // Find current process binary path

    TCHAR filename[1000];
    if (!GetModuleFileName(NULL, filename, 1000))
    {
        // Abort upgrade
        return;
    }

    // Rename current binary to archive name

    tstring archive_filename(filename);
    archive_filename += _T(".orig");

    bool bArchiveCreated = false;

    try
    {
        // We can't delete/modify the binary for a running Windows process,
        // so instead we move the running binary to an archive filename and
        // write the new version to the original filename.

        if (!DeleteFile(archive_filename.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
        {
            throw std::exception("Upgrade - DeleteFile failed");
        }

        if (!MoveFile(filename, archive_filename.c_str()))
        {
            throw std::exception("Upgrade - MoveFile failed");
        }

        bArchiveCreated = true;

        // Write new version to current binary file name

        AutoHANDLE file = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (file == INVALID_HANDLE_VALUE)
        {
            throw std::exception("Upgrade - CreateFile failed");
        }

        DWORD written;

        if (!WriteFile(file, download.c_str(), download.length(), &written, NULL) || written != download.length())
        {
            throw std::exception("Upgrade - WriteFile failed");
        }

        if (!FlushFileBuffers(file))
        {
            throw std::exception("Upgrade - FlushFileBuffers failed");
        }
    }
    catch (std::exception& ex)
    {
        std::stringstream s;
        s << ex.what() << " (" << GetLastError() << ")";
        my_print(NOT_SENSITIVE, false, s.str().c_str());

        // Try to restore the original version
        if (bArchiveCreated)
        {
            CopyFile(archive_filename.c_str(), filename, FALSE);
        }

        // Abort upgrade
        return;
    }

    m_upgradePending = true;
}

// Makes a thread-safe copy of m_currentSessionInfo
void ConnectionManager::CopyCurrentSessionInfo(SessionInfo& sessionInfo)
{
    AutoMUTEX lock(m_mutex);
    sessionInfo = m_currentSessionInfo;
}

// Makes a thread-safe copy of m_currentSessionInfo
void ConnectionManager::UpdateCurrentSessionInfo(const SessionInfo& sessionInfo)
{
    AutoMUTEX lock(m_mutex);
    m_currentSessionInfo = sessionInfo;

    try
    {
        TransportRegistry::AddServerEntries(
            m_currentSessionInfo.GetDiscoveredServerEntries(),
            // CoreTransport does not provide a ServerEntry, but VPNTransport does.
            m_currentSessionInfo.HasServerEntry() ? &m_currentSessionInfo.GetServerEntry() : 0);
    }
    catch (std::exception &ex)
    {
        my_print(NOT_SENSITIVE, false, string("HandleHandshakeResponse caught exception: ") + ex.what());
        // This isn't fatal.  The transport connection can still be established.
    }
}

struct FeedbackThreadData
{
    ConnectionManager* connectionManager;
    wstring feedbackJSON;
} g_feedbackThreadData;

void ConnectionManager::SendFeedback(LPCWSTR unicodeFeedbackJSON)
{
    g_feedbackThreadData.connectionManager = this;
    g_feedbackThreadData.feedbackJSON = unicodeFeedbackJSON;

    if (!m_feedbackThread ||
        WAIT_OBJECT_0 == WaitForSingleObject(m_feedbackThread, 0))
    {
        if (!(m_feedbackThread = CreateThread(
                                    0,
                                    0,
                                    ConnectionManager::ConnectionManagerFeedbackThread,
                                    (void*)&g_feedbackThreadData, 0, 0)))
        {
            my_print(NOT_SENSITIVE, false, _T("%s: CreateThread failed (%d)"), __TFUNCTION__, GetLastError());
            PostMessage(g_hWnd, WM_PSIPHON_FEEDBACK_FAILED, 0, 0);
            return;
        }
    }
}

DWORD WINAPI ConnectionManager::ConnectionManagerFeedbackThread(void* object)
{
    my_print(NOT_SENSITIVE, true, _T("%s: enter"), __TFUNCTION__);

    FeedbackThreadData* data = (FeedbackThreadData*)object;

    try
    {
        if (data->connectionManager->DoSendFeedback(data->feedbackJSON.c_str()))
        {
            PostMessage(g_hWnd, WM_PSIPHON_FEEDBACK_SUCCESS, 0, 0);
        }
        else
        {
            PostMessage(g_hWnd, WM_PSIPHON_FEEDBACK_FAILED, 0, 0);
        }
    }
    catch (StopSignal::StopException&)
    {
        // just fall through
    }

    my_print(NOT_SENSITIVE, true, _T("%s: exit"), __TFUNCTION__);
    return 0;
}

bool ConnectionManager::DoSendFeedback(LPCWSTR feedbackJSON)
{
    // NOTE: no lock while waiting for network events

    // Make a copy of SessionInfo for threadsafety.
    SessionInfo sessionInfo;
    {
        AutoMUTEX lock(m_mutex);
        sessionInfo = m_currentSessionInfo;
    }

    string narrowFeedbackJSON = WStringToUTF8(feedbackJSON);
    Json::Value json_entry;
    Json::Reader reader;
    if (!reader.parse(narrowFeedbackJSON, json_entry))
    {
        assert(0);
        return false;
    }

    string feedback = json_entry.get("feedback", "").asString();
    string email = json_entry.get("email", "").asString();
    bool sendDiagnosticInfo = json_entry.get("sendDiagnosticInfo", false).asBool();

    // Leave the survey responses as JSON.
    Json::Value surveyResponses = json_entry.get("responses", Json::nullValue);
    string surveyJSON = Json::FastWriter().write(surveyResponses);

    // When disconnected, ignore the user cancel flag in the HTTP request
    // wait loop.
    // TODO: the user may be left waiting too long after cancelling; add
    // a shorter timeout in this case
    DWORD stopReason = (GetState() == CONNECTION_MANAGER_STATE_CONNECTED ? STOP_REASON_ALL : STOP_REASON_NONE);

    bool success = true;

    // Upload diagnostic info
    if (!feedback.empty() || !surveyJSON.empty() || sendDiagnosticInfo)
    {
        // We don't care if this succeeds.
        success = SendFeedbackAndDiagnosticInfo(
                    feedback,
                    email,
                    surveyJSON,
                    sendDiagnosticInfo,
                    StopInfo(&GlobalStopSignal::Instance(), stopReason));
    }

    return success;
}
