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
    m_startingTime(0),
    m_transport(0),
    m_upgradePending(false),
    m_startSplitTunnel(false),
    m_nextFetchRemoteServerListAttempt(0)
{
    m_mutex = CreateMutex(NULL, FALSE, 0);

    InitializeUserSettings();
}

ConnectionManager::~ConnectionManager(void)
{
    Stop(STOP_REASON_NONE);
    CloseHandle(m_mutex);
}

ServerList& ConnectionManager::GetServerList()
{
    return m_serverList;
}

void ConnectionManager::OpenHomePages(const TCHAR* defaultHomePage/*=0*/)
{
    AutoMUTEX lock(m_mutex);
    
    if (!UserSkipBrowser())
    {
        vector<tstring> urls = m_currentSessionInfo.GetHomepages();
        if (urls.size() == 0 && defaultHomePage)
        {
            urls.push_back(defaultHomePage);
        }
        OpenBrowser(urls);
    }
}

void ConnectionManager::Toggle(const tstring& transport, bool startSplitTunnel)
{
    // NOTE: no lock, to allow thread to access object

    if (m_state == CONNECTION_MANAGER_STATE_STOPPED)
    {
        Start(transport,startSplitTunnel);
    }
    else
    {
        Stop(STOP_REASON_USER_DISCONNECT);
    }
}

time_t ConnectionManager::GetStartingTime()
{
    // NOTE: no lock, to prevent blocking connection thread with UI polling
    // Starting Time is informational only, consistency with state isn't critical

    if (m_state != CONNECTION_MANAGER_STATE_STARTING)
    {
        return 0;
    }
    else
    {
        return time(0) - m_startingTime;
    }
}

void ConnectionManager::SetState(ConnectionManagerState newState)
{
    // NOTE: no lock, to prevent blocking connection thread with UI polling
    // Starting Time is informational only, consistency with state isn't critical

    if (newState == CONNECTION_MANAGER_STATE_STARTING)
    {
        m_startingTime = time(0);
    }
    else
    {
        m_startingTime = 0;
    }

    m_state = newState;
}

ConnectionManagerState ConnectionManager::GetState()
{
    return m_state;
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

    my_print(NOT_SENSITIVE, true, _T("%s: exit"), __TFUNCTION__);
}

void ConnectionManager::FetchRemoteServerList(void)
{
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
                NarrowToTString(REMOTE_SERVER_LIST_ADDRESS).c_str(),
                443,
                "",
                NarrowToTString(REMOTE_SERVER_LIST_REQUEST_PATH).c_str(),
                response,
                StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_EXIT),
                false) // don't use local proxy
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
            false, 
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
        m_serverList.AddEntriesToList(newServerEntryVector, 0);
    }
    catch (std::exception &ex)
    {
        my_print(NOT_SENSITIVE, false, string("Corrupt remote server list: ") + ex.what());
        // This isn't fatal.
    }
}

void ConnectionManager::Start(const tstring& transport, bool startSplitTunnel)
{
    my_print(NOT_SENSITIVE, true, _T("%s: enter"), __TFUNCTION__);

    // Call Stop to cleanup in case thread failed on last Start attempt
    Stop(STOP_REASON_USER_DISCONNECT);

    AutoMUTEX lock(m_mutex);

    m_transport = TransportRegistry::New(transport);

    if (!m_transport->ServerWithCapabilitiesExists(GetServerList()))
    {
        my_print(NOT_SENSITIVE, false, _T("No servers support this protocol."));
        return;
    }

    m_startSplitTunnel = startSplitTunnel;

    GlobalStopSignal::Instance().ClearStopSignal(STOP_REASON_USER_DISCONNECT | STOP_REASON_UNEXPECTED_DISCONNECT);

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

void ConnectionManager::StartSplitTunnel()
{
    AutoMUTEX lock(m_mutex);

    if (m_splitTunnelRoutes.length() == 0)
    {
        tstring routesRequestPath = GetRoutesRequestPath(m_transport);

        SessionInfo sessionInfo;
        CopyCurrentSessionInfo(sessionInfo);
                
        string response;
        if (ServerRequest::MakeRequest(
                    ServerRequest::ONLY_IF_TRANSPORT,
                    m_transport,
                    sessionInfo,
                    routesRequestPath.c_str(),
                    response,
                    StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ALL)))
        {
            // Process split tunnel response
            ProcessSplitTunnelResponse(response);
        }
    }

    // Polipo is watching for changes to this file.
    // Note: there's some delay before the file change takes effect.
    WriteSplitTunnelRoutes(m_splitTunnelRoutes.c_str());
}

void ConnectionManager::StopSplitTunnel()
{
    AutoMUTEX lock(m_mutex);
    
    // See comment in StartSplitTunnel.
    DeleteSplitTunnelRoutes();
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

    //
    // Loop through server list, attempting to connect.
    //
    // When handshake and all connection types fail, the
    // server is marked as failed in the local server list and
    // the next server from the list is selected and retried.
    //
    // All operations may be interrupted by user cancel.
    //
    // NOTE: this function doesn't hold the ConnectionManager
    // object lock to allow for cancel etc.

    while (true) // Try servers loop
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

        try
        {
            GlobalStopSignal::Instance().CheckSignal(STOP_REASON_ALL, true);

            manager->SetState(CONNECTION_MANAGER_STATE_STARTING);

            // Get the next server to try

            tstring handshakeRequestPath;

            manager->LoadNextServer(handshakeRequestPath);

            // Note that the SessionInfo will only be partly filled in at this point.
            SessionInfo sessionInfo;
            manager->CopyCurrentSessionInfo(sessionInfo);

            // Record which server we're attempting to connect to
            ostringstream ss;
            ss << "ipAddress: " << sessionInfo.GetServerAddress();
            AddDiagnosticInfoYaml("ConnectingServer", ss.str().c_str());

            // We're looping around to run again. We're assuming that the calling
            // function knows that there's at least one server to try. We're 
            // not reporting anything, as the user doesn't need to know what's
            // going on under the hood at this point.
            if (!manager->m_transport->ServerHasCapabilities(sessionInfo.GetServerEntry()))
            {
                my_print(NOT_SENSITIVE, true, _T("%s: serverHasCapabilities failed"), __TFUNCTION__);

                manager->MarkCurrentServerFailed();

                continue;
            }

            //
            // Set up the transport connection
            //

            my_print(NOT_SENSITIVE, true, _T("%s: doing transportConnection for %s"), __TFUNCTION__, manager->m_transport->GetTransportDisplayName().c_str());

            // Note that the TransportConnection will do any necessary cleanup.
            TransportConnection transportConnection;

            // May throw TryNextServer
            transportConnection.Connect(
                StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ALL),
                manager->m_transport,
                manager,
                sessionInfo,
                handshakeRequestPath.c_str(),
                manager->GetSplitTunnelingFilePath());

            //
            // The transport connection did a handshake, so its sessionInfo is 
            // fuller than ours. Update ours and then update the server entries.
            //

            sessionInfo = transportConnection.GetUpdatedSessionInfo();
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

            // If the stop signal hasn't been set, then this is an unexpected 
            // disconnect. In which case, fail over and retry.
            if (!GlobalStopSignal::Instance().CheckSignal(STOP_REASON_ALL, false))
            {
                throw TransportConnection::TryNextServer();
            }

            //
            // Disconnected
            //

            manager->SetState(CONNECTION_MANAGER_STATE_STOPPED);

            my_print(NOT_SENSITIVE, true, _T("%s: breaking"), __TFUNCTION__);
            break;
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
        catch (TransportConnection::TryNextServer&)
        {
            // Failed to connect to the server. Try the next one.
            my_print(NOT_SENSITIVE, true, _T("%s: caught TryNextServer"), __TFUNCTION__);

            manager->SetState(CONNECTION_MANAGER_STATE_STARTING);

            manager->MarkCurrentServerFailed();

            // Give users some feedback. Before, when the handshake failed
            // all we displayed was "WinHttpCallbackFailed (200000)" and kept
            // the arrow animation spinning. A user-authored FAQ mentioned
            // this error in particular and recommended waiting. So here's
            // a lightly more encouraging message.
            my_print(NOT_SENSITIVE, false, _T("Trying next server..."));

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
    }

    my_print(NOT_SENSITIVE, true, _T("%s: exiting thread"), __TFUNCTION__);
    return 0;
}

void ConnectionManager::DoPostConnect(const SessionInfo& sessionInfo, bool openHomePages)
{
    // Called from connection thread
    // NOTE: no lock while waiting for network events

    SetState(CONNECTION_MANAGER_STATE_CONNECTED);

    //
    // "Connected" HTTPS request for server stats and split tunnel routing info.
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


#ifdef SPEEDTEST
        // Speed feedback
        // Note: the /connected request *is* tunneled

        DWORD now = GetTickCount();
        if (now >= start) // GetTickCount can wrap
        {
            string speedResponse;
            (void)ServerRequest::MakeRequest(
                            ServerRequest::ONLY_IF_TRANSPORT,
                            m_transport,
                            sessionInfo,
                            GetSpeedRequestPath(
                                m_transport->GetTransportProtocolName(),
                                _T("connected"),
                                _T(""),
                                now-start,
                                response.length()).c_str(),
                            speedResponse,
                            StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ALL));
        }
#endif //SPEEDTEST

        // Process flag to start split tunnel after initial connection
        if (m_transport->IsSplitTunnelSupported() && m_startSplitTunnel)
        {
            StartSplitTunnel();
        }
    }

    //
    // Open home pages in browser
    //
    
    if (openHomePages)
    {
        OpenHomePages();
    }

#ifdef SPEEDTEST
    // Perform non-tunneled speed test when requested
    // Note that in VPN mode, the WinHttp request is implicitly tunneled.

    tstring speedTestServerAddress, speedTestRequestPath;
    int speedTestServerPort = 0;
    GetSpeedTestURL(speedTestServerAddress, speedTestServerPort, speedTestRequestPath);
    // HTTPSRequest is always https
    tstringstream speedTestURL;
    speedTestURL << _T("https://") << speedTestServerAddress << _T(":") << speedTestServerPort << speedTestRequestPath;

    if (speedTestServerAddress.length() > 0)
    {
        DWORD start = GetTickCount();
        string response;
        HTTPSRequest httpsRequest;
        bool success = false;
        if (httpsRequest.MakeRequest(
                            speedTestServerAddress.c_str(),
                            speedTestServerPort,
                            "",
                            speedTestRequestPath.c_str(),
                            response,
                            // Because it's not tunneled, in theory this doesn't 
                            // need to be STOP_REASON_ALL -- it could instead be 
                            // _EXIT. But we spawn a new speed test on each 
                            // connection, so we'd better clean this up each time
                            // the connection comes down (before the next comes up).
                            StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ALL),
                            false)) // don't proxy
        {
            success = true;
        }
        DWORD now = GetTickCount();
        if (now >= start) // GetTickCount can wrap
        {
            string speedResponse;
            (void)ServerRequest::MakeRequest(
                            ServerRequest::ONLY_IF_TRANSPORT,
                            m_transport,
                            sessionInfo,
                            GetSpeedRequestPath(
                                m_transport->GetTransportProtocolName(),
                                success ? _T("speed_test") : _T("speed_test_failure"),
                                speedTestURL.str().c_str(),
                                now-start,
                                response.length()).c_str(),
                            speedResponse,
                            StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ALL));
        }
    }
#endif //SPEEDTEST
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

tstring ConnectionManager::GetSpeedRequestPath(const tstring& relayProtocol, const tstring& operation, const tstring& info, DWORD milliseconds, DWORD size)
{
    AutoMUTEX lock(m_mutex);

    std::stringstream strMilliseconds;
    strMilliseconds << milliseconds;

    std::stringstream strSize;
    strSize << size;

    return tstring(HTTP_SPEED_REQUEST_PATH) + 
           _T("?client_session_id=") + NarrowToTString(m_currentSessionInfo.GetClientSessionID()) +
           _T("&propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") + relayProtocol +
           _T("&operation=") + operation +
           _T("&info=") + info +
           _T("&milliseconds=") + NarrowToTString(strMilliseconds.str()) +
           _T("&size=") + NarrowToTString(strSize.str());
}

void ConnectionManager::GetSpeedTestURL(tstring& serverAddress, int& serverPort, tstring& requestPath)
{
    AutoMUTEX lock(m_mutex);

    serverAddress = NarrowToTString(m_currentSessionInfo.GetSpeedTestServerAddress());
    serverPort = m_currentSessionInfo.GetSpeedTestServerPort();
    requestPath = NarrowToTString(m_currentSessionInfo.GetSpeedTestRequestPath());
}

tstring ConnectionManager::GetFailedRequestPath(ITransport* transport)
{
    AutoMUTEX lock(m_mutex);

    return tstring(HTTP_FAILED_REQUEST_PATH) + 
           _T("?client_session_id=") + NarrowToTString(m_currentSessionInfo.GetClientSessionID()) +
           _T("&propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") +  transport->GetTransportProtocolName() + 
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
           _T("?client_session_id=") + NarrowToTString(m_currentSessionInfo.GetClientSessionID()) +
           _T("&propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") + transport->GetTransportProtocolName() + 
           _T("&session_id=") + transport->GetSessionID(m_currentSessionInfo) +
           _T("&last_connected=") + NarrowToTString(lastConnected);
}

tstring ConnectionManager::GetRoutesRequestPath(ITransport* transport)
{
    AutoMUTEX lock(m_mutex);

    return tstring(HTTP_ROUTES_REQUEST_PATH) + 
           _T("?client_session_id=") + NarrowToTString(m_currentSessionInfo.GetClientSessionID()) +
           _T("&propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") +  (transport ? transport->GetTransportProtocolName() : _T("")) + 
           _T("&session_id=") + (transport ? transport->GetSessionID(m_currentSessionInfo) : _T(""));
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
           _T("?client_session_id=") + NarrowToTString(m_currentSessionInfo.GetClientSessionID()) +
           _T("&propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") +  transport->GetTransportProtocolName() + 
           _T("&session_id=") + sessionID + 
           _T("&connected=") + (connected ? _T("1") : _T("0"));
}

void ConnectionManager::GetUpgradeRequestInfo(SessionInfo& sessionInfo, tstring& requestPath)
{
    AutoMUTEX lock(m_mutex);

    sessionInfo = m_currentSessionInfo;
    requestPath = tstring(HTTP_DOWNLOAD_REQUEST_PATH) + 
                    _T("?client_session_id=") + NarrowToTString(m_currentSessionInfo.GetClientSessionID()) +
                    _T("&propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
                    _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
                    _T("&client_version=") + NarrowToTString(m_currentSessionInfo.GetUpgradeVersion()) +
                    _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret());
}

tstring ConnectionManager::GetFeedbackRequestPath(ITransport* transport)
{
    AutoMUTEX lock(m_mutex);

    return tstring(HTTP_FEEDBACK_REQUEST_PATH) + 
           _T("?client_session_id=") + NarrowToTString(m_currentSessionInfo.GetClientSessionID()) +
           _T("&propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") +  (transport ? transport->GetTransportProtocolName() : _T("")) + 
           _T("&session_id=") + (transport ? transport->GetSessionID(m_currentSessionInfo) : _T("")) + 
           _T("&connected=") + ((GetState() == CONNECTION_MANAGER_STATE_CONNECTED) ? _T("1") : _T("0"));
}

void ConnectionManager::MarkCurrentServerFailed(void)
{
    AutoMUTEX lock(m_mutex);
    
    m_serverList.MarkServerFailed(m_currentSessionInfo.GetServerAddress());
}

// ==== General Session Functions =============================================

// Note that the SessionInfo structure will only be partly filled in by this function.
void ConnectionManager::LoadNextServer(tstring& handshakeRequestPath)
{
    // Select next server to try to connect to

    AutoMUTEX lock(m_mutex);

    ServerEntry serverEntry;
    
    try
    {
        // Try the next server in our list.
        serverEntry = m_serverList.GetNextServer();
    }
    catch (std::exception &ex)
    {
        my_print(NOT_SENSITIVE, false, string("LoadNextServer caught exception: ") + ex.what());
        throw Abort();
    }

    // Ensure split tunnel routes are reset before new session
    m_splitTunnelRoutes = "";
    StopSplitTunnel();

    // Current session holds server entry info and will also be loaded
    // with homepage and other info.
    m_currentSessionInfo.Set(serverEntry);

    // Generate a new client session ID to be included with all subsequent web requests
    m_currentSessionInfo.GenerateClientSessionID();

    // Output values used in next TryNextServer step

    handshakeRequestPath = tstring(HTTP_HANDSHAKE_REQUEST_PATH) + 
                           _T("?client_session_id=") + NarrowToTString(m_currentSessionInfo.GetClientSessionID()) +
                           _T("&propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
                           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
                           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
                           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
                           _T("&relay_protocol=") + m_transport->GetTransportProtocolName();

    // Include a list of known server IP addresses in the request query string as required by /handshake
    ServerEntries serverEntries =  m_serverList.GetList();
    for (ServerEntryIterator ii = serverEntries.begin(); ii != serverEntries.end(); ++ii)
    {
        handshakeRequestPath += _T("&known_server=");
        handshakeRequestPath += NarrowToTString(ii->serverAddress);
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
                NarrowToTString(UPGRADE_ADDRESS).c_str(),
                443,
                "",
                NarrowToTString(UPGRADE_REQUEST_PATH).c_str(),
                downloadResponse,
                StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ALL))
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

#ifdef SPEEDTEST
            // Speed feedback
            DWORD now = GetTickCount();
            if (now >= start) // GetTickCount can wrap
            {
                string speedResponse;
                (void)ServerRequest::MakeRequest( // Ignore failure
                                ServerRequest::ONLY_IF_TRANSPORT,
                                manager->m_transport,
                                sessionInfo,
                                manager->GetSpeedRequestPath(
                                    manager->m_transport->GetTransportProtocolName(),
                                    _T("download"),
                                    _T(""),
                                    now-start,
                                    downloadResponse.length()).c_str(),
                                speedResponse,
                                StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ALL));
            }
#endif //SPEEDTEST

            // Perform upgrade.

            string upgradeData;

            if (verifySignedDataPackage(
                    UPGRADE_SIGNATURE_PUBLIC_KEY,
                    downloadResponse.c_str(), 
                    downloadResponse.length(),
                    true, // compressed
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

void ConnectionManager::ProcessSplitTunnelResponse(const string& compressedRoutes)
{
    AutoMUTEX lock(m_mutex);

    // Decompress split tunnel route info
    // Defaults to blank route list on any error --> no split tunneling

    m_splitTunnelRoutes = "";

    if (compressedRoutes.length() == 0)
    {
        return;
    }

    const int CHUNK_SIZE = 1024;
    const int SANITY_CHECK_SIZE = 10*1024*1024;
    int ret;
    z_stream stream;
    char out[CHUNK_SIZE+1];

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = compressedRoutes.length();
    stream.next_in = (unsigned char*)compressedRoutes.c_str();

    if (Z_OK != inflateInit(&stream))
    {
        return;
    }

    do
    {
        stream.avail_out = CHUNK_SIZE;
        stream.next_out = (unsigned char*)out;
        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END)
        {
            my_print(NOT_SENSITIVE, true, _T("ProcessSplitTunnelResponse failed (%d)"), ret);
            m_splitTunnelRoutes = "";
            break;
        }

        out[CHUNK_SIZE - stream.avail_out] = '\0';

        m_splitTunnelRoutes += out;

        if (m_splitTunnelRoutes.length() > SANITY_CHECK_SIZE)
        {
            my_print(NOT_SENSITIVE, true, _T("ProcessSplitTunnelResponse overflow"));
            m_splitTunnelRoutes = "";
            break;
        }

    } while (ret != Z_STREAM_END);

    inflateEnd(&stream);
}

tstring ConnectionManager::GetSplitTunnelingFilePath()
{
    TCHAR filePath[MAX_PATH];
    TCHAR tempPath[MAX_PATH];
    // http://msdn.microsoft.com/en-us/library/aa364991%28v=vs.85%29.aspx notes
    // tempPath can contain no more than MAX_PATH-14 characters
    int ret = GetTempPath(MAX_PATH, tempPath);
    if (ret > MAX_PATH-14 || ret == 0)
    {
        return _T("");
    }

    if(NULL != PathCombine(filePath, tempPath, SPLIT_TUNNELING_FILE_NAME))
    {
        return tstring(filePath);
    }
    return _T("");
}

bool ConnectionManager::WriteSplitTunnelRoutes(const char* routes)
{
    AutoMUTEX lock(m_mutex);

    tstring filePath = GetSplitTunnelingFilePath();
    if (filePath.length() == 0)
    {
        my_print(NOT_SENSITIVE, false, _T("WriteSplitTunnelRoutes - GetSplitTunnelingFilePath failed (%d)"), GetLastError());
        return false;
    }

    AutoHANDLE file = CreateFile(filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        my_print(NOT_SENSITIVE, false, _T("WriteSplitTunnelRoutes - CreateFile failed (%d)"), GetLastError());
        return false;
    }

    DWORD length = strlen(routes);
    DWORD written;
    if (!WriteFile(
            file,
            (unsigned char*)routes,
            length,
            &written,
            NULL)
          || written != length)
    {
        my_print(NOT_SENSITIVE, false, _T("WriteSplitTunnelRoutes - WriteFile failed (%d)"), GetLastError());
        return false;
    }

    return true;
}

bool ConnectionManager::DeleteSplitTunnelRoutes()
{
    AutoMUTEX lock(m_mutex);

    tstring filePath = GetSplitTunnelingFilePath();
    if (filePath.length() == 0)
    {
        my_print(NOT_SENSITIVE, false, _T("DeleteSplitTunnelRoutes - GetSplitTunnelingFilePath failed (%d)"), GetLastError());
        return false;
    }

    if (!DeleteFile(filePath.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
    {
        my_print(NOT_SENSITIVE, false, _T("DeleteSplitTunnelRoutes - DeleteFile failed (%d)"), GetLastError());
        return false;
    }

    return true;
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
        m_serverList.AddEntriesToList(
            m_currentSessionInfo.GetDiscoveredServerEntries(), 
            &m_currentSessionInfo.GetServerEntry());
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

void ConnectionManager::SendFeedback(LPCWSTR feedbackJSON)
{
    g_feedbackThreadData.connectionManager = this;
    g_feedbackThreadData.feedbackJSON = feedbackJSON;

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

    if (data->connectionManager->DoSendFeedback(data->feedbackJSON.c_str()))
    {
        PostMessage(g_hWnd, WM_PSIPHON_FEEDBACK_SUCCESS, 0, 0);
    }
    else
    {
        PostMessage(g_hWnd, WM_PSIPHON_FEEDBACK_FAILED, 0, 0);
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

    // Two different actions might be required at this point:
    // 1) The user wishes to send freeform feedback text (optionally uploading 
    //    diagnostic info).
    // 2) The user completed the questionnaire and wishes to submit it 
    //    (optionally uploading diagnostic info).

    // Upload diagnostic info
    if (!feedback.empty() || sendDiagnosticInfo) 
    {
        // We don't care if this succeeds.
        (void)SendFeedbackAndDiagnosticInfo(
                feedback,
                email,
                surveyJSON,
                sendDiagnosticInfo,
                StopInfo(&GlobalStopSignal::Instance(), stopReason));
    }

    if (surveyResponses != Json::nullValue)
    {
        // Send the feedback questionnaire responses.
        // Note that we send the entire JSON string, even though the server
        // (at this time) only wants the 'responses' sub-structure.

        tstring requestPath = GetFeedbackRequestPath(m_transport);
        string response;

        success = ServerRequest::MakeRequest(
                            ServerRequest::NO_TEMP_TUNNEL,
                            m_transport,
                            sessionInfo,
                            requestPath.c_str(),
                            response,
                            StopInfo(&GlobalStopSignal::Instance(), stopReason),
                            L"Content-Type: application/json",
                            (LPVOID)narrowFeedbackJSON.c_str(),
                            narrowFeedbackJSON.length());
    }

    return success;
}
