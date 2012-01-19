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
#include "httpsrequest.h"
#include "webbrowser.h"
#include "embeddedvalues.h"
#include "sshconnection.h"
#include "usersettings.h"
#include "zlib.h"
#include <algorithm>
#include <sstream>


// Upgrade process posts a Quit message
extern HWND g_hWnd;


ConnectionManager::ConnectionManager(void) :
    m_state(CONNECTION_MANAGER_STATE_STOPPED),
    m_userSignalledStop(false),
    m_sshConnection(m_userSignalledStop),
    m_thread(0),
    m_currentSessionSkippedVPN(false),
    m_startingTime(0)
{
    m_mutex = CreateMutex(NULL, FALSE, 0);

    InitializeUserSettings();
}

ConnectionManager::~ConnectionManager(void)
{
    Stop();
    CloseHandle(m_mutex);
}

void ConnectionManager::OpenHomePages(void)
{
    AutoMUTEX lock(m_mutex);
    
    if (!UserSkipBrowser())
    {
        OpenBrowser(m_currentSessionInfo.GetHomepages());
    }
}

void ConnectionManager::SetSkipVPN(void)
{
    AutoMUTEX lock(m_mutex);

    m_vpnList.SetSkipVPN();
}

void ConnectionManager::ResetSkipVPN(void)
{
    AutoMUTEX lock(m_mutex);

    m_vpnList.ResetSkipVPN();
}

bool ConnectionManager::GetSkipVPN(void)
{
    AutoMUTEX lock(m_mutex);

    return m_vpnList.GetSkipVPN();
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
        Stop();
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

ConnectionManagerState ConnectionManager::GetState(void)
{
    return m_state;
}

void ConnectionManager::Stop(void)
{
    // NOTE: no lock, to allow thread to access object

    // The assumption is that signalling stop will cause any current operations to
    // stop (such as making HTTPS requests, or establishing a connection), and
    // cause the connection to hang up if it is connected.
    // While a connection is active, there is a thread running waiting for the
    // connection to terminate.

    // Cancel flag is also termination flag
    m_userSignalledStop = true;

    // Wait for thread to exit (otherwise can get access violation when app terminates)
    if (m_thread)
    {
        WaitForSingleObject(m_thread, INFINITE);
        m_thread = 0;
    }
}

void ConnectionManager::Start(void)
{
    // Call Stop to cleanup in case thread failed on last Start attempt
    Stop();

    AutoMUTEX lock(m_mutex);

    m_userSignalledStop = false;

    if (m_state != CONNECTION_MANAGER_STATE_STOPPED || m_thread != 0)
    {
        my_print(false, _T("Invalid connection manager state in Start (%d)"), m_state);
        return;
    }

    SetState(CONNECTION_MANAGER_STATE_STARTING);

    if (!(m_thread = CreateThread(0, 0, ConnectionManagerStartThread, (void*)this, 0, 0)))
    {
        my_print(false, _T("Start: CreateThread failed (%d)"), GetLastError());

        SetState(CONNECTION_MANAGER_STATE_STOPPED);
    }
}

void ConnectionManager::DoVPNConnection(
        ConnectionManager* manager,
        const ServerEntry& serverEntry)
{
    // NOTE: this function is a helper for ConnectionManagerStartThread and so doesn't hold the lock

    if (!manager->CurrentServerVPNCapable())
    {
        throw TryNextServer();
    }

    //
    // Minimum version check for VPN
    // - L2TP/IPSec/PSK not supported on Windows 2000
    // - Throws to try next server -- an assumption here is we'll always try SSH next
    //
    
    OSVERSIONINFO versionInfo;
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    if (!GetVersionEx(&versionInfo) ||
            versionInfo.dwMajorVersion < 5 ||
            (versionInfo.dwMajorVersion == 5 && versionInfo.dwMinorVersion == 0))
    {
        my_print(false, _T("VPN requires Windows XP or greater"));
        throw TryNextServer();
    }
    
    //
    // Check VPN services and fix if required/possible
    //
    
    // Note: we proceed even if the call fails. Testing is inconsistent -- don't
    // always need all tweaks to connect.
    TweakVPN();
    
    //
    // Start VPN connection
    //
    
    manager->VPNEstablish();
    
    //
    // Monitor VPN connection and wait for CONNECTED or FAILED
    //
    
    manager->WaitForVPNConnectionStateToChangeFrom(VPN_CONNECTION_STATE_STARTING);
    
    if (VPN_CONNECTION_STATE_CONNECTED != manager->GetVPNConnectionState())
    {
        // Note: WaitForVPNConnectionStateToChangeFrom throws Abort if user
        // cancelled, so if we're here it's a FAILED case.
    
        // Report error code to server for logging/trouble-shooting.
        // The request line includes the last VPN error code.
        
        tstring requestPath = manager->GetVPNFailedRequestPath();
    
        string response;
        HTTPSRequest httpsRequest;
        if (!httpsRequest.MakeRequest(
                            manager->GetUserSignalledStop(),
                            NarrowToTString(serverEntry.serverAddress).c_str(),
                            serverEntry.webServerPort,
                            serverEntry.webServerCertificate,
                            requestPath.c_str(),
                            response))
        {
            // Ignore failure
        }
    
        throw TryNextServer();
    }
    
    manager->SetState(CONNECTION_MANAGER_STATE_CONNECTED_VPN);
    
    //
    // Patch DNS bug on Windowx XP; and flush DNS
    // to ensure domains are resolved with VPN's DNS server
    //
    
    // Note: we proceed even if the call fails. This means some domains
    // may not resolve properly.
    TweakDNS();
    
    //
    // "Connected" HTTPS request for server stats and split tunnel routing info.
    // It's not critical if this request fails so failure is ignored.
    //
    
    tstring connectedRequestPath = manager->GetVPNConnectRequestPath();
        
    string response;
    HTTPSRequest httpsRequest;
    if (httpsRequest.MakeRequest(
                        manager->GetUserSignalledStop(),
                        NarrowToTString(serverEntry.serverAddress).c_str(),
                        serverEntry.webServerPort,
                        serverEntry.webServerCertificate,
                        connectedRequestPath.c_str(),
                        response))
    {
        // Process split tunnel response
        manager->ProcessSplitTunnelResponse(response);
    }

    //
    // Open home pages in browser
    //
    
    manager->OpenHomePages();

    //
    // Wait for VPN connection to stop (or fail) -- set ConnectionManager state accordingly (used by UI)
    //
    
    manager->WaitForVPNConnectionStateToChangeFrom(VPN_CONNECTION_STATE_CONNECTED);
    
    manager->SetState(CONNECTION_MANAGER_STATE_STOPPED);
}

void ConnectionManager::DoSSHConnection(
        ConnectionManager* manager,
        const ServerEntry& serverEntry)
{
    // NOTE: this function is a helper for ConnectionManagerStartThread and so doesn't hold the lock

    if (!manager->CurrentServerSSHCapable())
    {
        throw TryNextServer();
    }

    //
    // Establish SSH connection
    //
    // First attempt is using obfuscated SSH port and protocol, second attempt is using
    // standard SSH port and protocol (in case the unusual configuration is blocked and
    // the standard configuration is not).
    //

    bool connected = false;
    int connectType = SSH_CONNECT_OBFUSCATED;

    for (; connectType <= SSH_CONNECT_STANDARD; connectType++)
    {
        if (!manager->SSHConnect(connectType) || !manager->SSHWaitForConnected())
        {
            // Explicit disconnect cleanup before next attempt (SSH object cleans
            // up automatically, but that results in confusing log output).

            manager->SSHDisconnect();

            if (manager->GetUserSignalledStop())
            {
                throw Abort();
            }

            // Report error code to server for logging/trouble-shooting.
        
            tstring requestPath = manager->GetSSHFailedRequestPath(connectType);
    
            string response;
            HTTPSRequest httpsRequest;
            if (!httpsRequest.MakeRequest(
                                manager->GetUserSignalledStop(),
                                NarrowToTString(serverEntry.serverAddress).c_str(),
                                serverEntry.webServerPort,
                                serverEntry.webServerCertificate,
                                requestPath.c_str(),
                                response))
            {
                // Ignore failure
            }
        }
        else
        {
            connected = true;
            break;
        }
    }

    if (!connected)
    {
        // Neither attempt succeeded

        throw TryNextServer();
    }

    manager->SetState(CONNECTION_MANAGER_STATE_CONNECTED_SSH);


    //
    // "Connected" HTTPS request for server stats and split tunnel routing info.
    // It's not critical if this request fails so failure is ignored.
    //
    
    tstring connectedRequestPath = manager->GetSSHConnectRequestPath(connectType);
        
    string response;
    HTTPSRequest httpsRequest;
    if (httpsRequest.MakeRequest(
                        manager->GetUserSignalledStop(),
                        NarrowToTString(serverEntry.serverAddress).c_str(),
                        serverEntry.webServerPort,
                        serverEntry.webServerCertificate,
                        connectedRequestPath.c_str(),
                        response))
    {
        // Process split tunnel response
        manager->ProcessSplitTunnelResponse(response);
    }

    //
    // Open home pages in browser
    //
   
    manager->OpenHomePages();    

    //
    // Wait for SSH connection to stop (or fail)
    //

    // Note: doesn't throw abort on user cancel, but it all works out the same
    manager->SSHWaitAndDisconnect();
    
    manager->SetState(CONNECTION_MANAGER_STATE_STOPPED);
}

DWORD WINAPI ConnectionManager::ConnectionManagerStartThread(void* data)
{
    ConnectionManager* manager = (ConnectionManager*)data;

    // Seed built-in non-crypto PRNG used for shuffling (load balancing)
    unsigned int seed = (unsigned)time(NULL);
    srand(seed);

    // Loop through server list, attempting to connect.
    //
    // Connect sequence:
    //
    // - Make Handshake HTTPS request
    // - Perform download HTTPS request and upgrade, if applicable
    // - Try VPN:
    // -- Create and dial VPN connection
    // -- Tweak VPN system settings if required
    // -- Wait for VPN connection to succeed or fail
    // -- Flush DNS and fix settings if required
    // - If VPN failed:
    // -- Create SSH connection
    // -- Wait for SSH connection to succeed or fail
    // - If a connection type succeeded:
    // -- Launch home pages (failure is acceptable)
    // -- Make Connected HTTPS request (failure is acceptable)
    // -- Wait for connection to stop
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
        try
        {
            // Ensure UI doesn't show "VPN Skipped" icon
            manager->SetCurrentConnectionSkippedVPN(false);

            //
            // Handshake HTTPS request
            //

            ServerEntry serverEntry;
            tstring handshakeRequestPath;
            string handshakeResponse;

            // Send list of known server IP addresses (used for stats logging on the server)

            manager->LoadNextServer(
                            serverEntry,
                            handshakeRequestPath);

            // If the "Skip VPN" flag is set, the last time the user
            // connected with this server, VPN failed. So we don't
            // try VPN again.
            // Note: this flag is cleared whenever the first server
            // in the list changes, so VPN will be tried again.

            bool skipVPN = manager->GetSkipVPN();

            HTTPSRequest httpsRequest;
            if (!httpsRequest.MakeRequest(
                                manager->GetUserSignalledStop(),
                                NarrowToTString(serverEntry.serverAddress).c_str(),
                                serverEntry.webServerPort,
                                serverEntry.webServerCertificate,
                                handshakeRequestPath.c_str(),
                                handshakeResponse))
            {
                if (manager->GetUserSignalledStop())
                {
                    throw Abort();
                }

                // We now have the client retry on port 443 in case the
                // configured port is blocked. If this works, then 443
                // is used for subsequent web requests.

                // TODO: the client could 'remember' which port works
                // and skip the blocked one next time to avoid waiting
                // for inevitable timeouts.

                else if (serverEntry.webServerPort != 443
                            && httpsRequest.MakeRequest(
                                        manager->GetUserSignalledStop(),
                                        NarrowToTString(serverEntry.serverAddress).c_str(),
                                        443,
                                        serverEntry.webServerCertificate,
                                        handshakeRequestPath.c_str(),
                                        handshakeResponse))
                {
                    serverEntry.webServerPort = 443;
                    // Fall through to success case
                }
                else
                {
                    throw TryNextServer();
                }
            }

            manager->HandleHandshakeResponse(handshakeResponse.c_str());

            //
            // Upgrade
            //

            // Upgrade now if handshake notified of new version
            tstring downloadRequestPath;
            string downloadResponse;
            if (manager->RequireUpgrade(downloadRequestPath))
            {
                // Download new binary

                if (!httpsRequest.MakeRequest(
                            manager->GetUserSignalledStop(),
                            NarrowToTString(serverEntry.serverAddress).c_str(),
                            serverEntry.webServerPort,
                            serverEntry.webServerCertificate,
                            downloadRequestPath.c_str(),
                            downloadResponse))
                {
                    if (manager->GetUserSignalledStop())
                    {
                        throw Abort();
                    }
                    // else fall through to Establish()

                    // If the download failed, we simply proceed with the connection.
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
                    // Perform upgrade.
        
                    // If the upgrade succeeds, it will terminate the process and we don't proceed with Establish.
                    // If it fails, we DO proceed with Establish -- using the old (current) version.  One scenario
                    // in this case is if the binary is on read-only media.
                    // NOTE: means the server should always support old versions... which for now just means
                    // supporting Establish() etc. as we're already past the handshake.

                    if (manager->DoUpgrade(downloadResponse))
                    {
                        // NOTE: state will remain INITIALIZING.  The app is terminating.
                        return 0;
                    }
                    // else fall through to Establish()
                }
            }

            bool userSkipVPN = UserSkipVPN();

            try
            {
                // Establish VPN connection and wait for termination
                // Throws TryNextServer or Abort on failure
                
                manager->SetCurrentConnectionSkippedVPN(skipVPN);

                // NOTE: IGNORE_VPN_RELAY is for automated testing only

                if (IGNORE_VPN_RELAY || skipVPN || userSkipVPN)
                {
                    throw TryNextServer();
                }

                DoVPNConnection(manager, serverEntry);
            }
            catch (TryNextServer&)
            {
                // When the VPN attempt fails, establish SSH connection and wait for termination
                manager->RemoveVPNConnection();

                // Don't set the persistent flag if the user setting is set, so that removing the
                // user setting will cause VPN to be tried again.
                if (!userSkipVPN)
                {
                    // Set persistent flag to not try VPN again when we run exactly the same server again
                    manager->SetSkipVPN();
                }

                DoSSHConnection(manager, serverEntry);
            }

            break;
        }
        catch (Abort&)
        {
            manager->RemoveVPNConnection();
            manager->SSHDisconnect();
            manager->SetState(CONNECTION_MANAGER_STATE_STOPPED);
            break;
        }
        catch (TryNextServer&)
        {
            manager->RemoveVPNConnection();
            manager->SSHDisconnect();
            manager->MarkCurrentServerFailed();

            // Give users some feedback. Before, when the handshake failed
            // all we displayed was "WinHttpCallbackFailed (200000)" and kept
            // the arrow animation spinning. A user-authored FAQ mentioned
            // this error in particular and recommended waiting. So here's
            // a lightly more encouraging message.
            my_print(false, _T("Trying next server..."));

            // Continue while loop to try next server

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

    return 0;
}

void ConnectionManager::SendStatusMessage(
                            int connectType, bool connected,
                            const map<string, int>& pageViewEntries,
                            const map<string, int>& httpsRequestEntries,
                            unsigned long long bytesTransferred)
{
    // NOTE: no lock while waiting for network events

    string serverAddress;
    int webServerPort;
    string webServerCertificate;
    {
        AutoMUTEX lock(m_mutex);
        serverAddress = m_currentSessionInfo.GetServerAddress();
        webServerPort = m_currentSessionInfo.GetWebPort();
        webServerCertificate = m_currentSessionInfo.GetWebServerCertificate();
    }

    // When disconnected, ignore the user cancel flag in the HTTP request
    // wait loop.
    // TODO: the user may be left waiting too long after cancelling; add
    // a shorter timeout in this case
    bool ignoreCancel = false;
    bool& cancel = connected ? GetUserSignalledStop() : ignoreCancel;

    // Format stats data for consumption by the server. 

    Json::Value stats;
    stats["bytes_transferred"] = bytesTransferred;
    my_print(true, _T("BYTES: %llu"), bytesTransferred);

    map<string, int>::const_iterator pos = pageViewEntries.begin();
    Json::Value page_views(Json::arrayValue);
    for (; pos != pageViewEntries.end(); pos++)
    {
        Json::Value entry;
        entry["page"] = pos->first;
        entry["count"] = pos->second;
        page_views.append(entry);
        my_print(true, _T("PAGEVIEW: %d: %S"), pos->second, pos->first.c_str());
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
        my_print(true, _T("HTTPS REQUEST: %d: %S"), pos->second, pos->first.c_str());
    }
    stats["https_requests"] = https_requests;

    ostringstream additionalData; 
    additionalData << stats; 
    string additionalDataString = additionalData.str();
    my_print(true, _T("%s:%d - PAGE VIEWS JSON: %S"), __TFUNCTION__, __LINE__, additionalDataString.c_str());

    tstring requestPath = GetSSHStatusRequestPath(connectType, connected);
    string response;
    HTTPSRequest httpsRequest;
    httpsRequest.MakeRequest(
            cancel,
            NarrowToTString(serverAddress).c_str(),
            webServerPort,
            webServerCertificate,
            requestPath.c_str(),
            response,
            L"Content-Type: application/json",
            (LPVOID)additionalDataString.c_str(),
            additionalDataString.length());
    
    // status message is for stats, success isn't critical
}

// ==== VPN Session Functions =================================================

bool ConnectionManager::CurrentServerVPNCapable()
{
    AutoMUTEX lock(m_mutex);

    return m_currentSessionInfo.GetPSK().length() > 0;
}

tstring ConnectionManager::GetVPNConnectRequestPath(void)
{
    AutoMUTEX lock(m_mutex);

    return tstring(HTTP_CONNECTED_REQUEST_PATH) + 
           _T("?propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=VPN") +
           _T("&session_id=") + m_vpnConnection.GetPPPIPAddress();
}

tstring ConnectionManager::GetVPNFailedRequestPath(void)
{
    AutoMUTEX lock(m_mutex);

    std::stringstream s;
    s << m_vpnConnection.GetLastVPNErrorCode();

    return tstring(HTTP_FAILED_REQUEST_PATH) + 
           _T("?propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=VPN") +
           _T("&error_code=") + NarrowToTString(s.str());
}

VPNConnectionState ConnectionManager::GetVPNConnectionState(void)
{
    AutoMUTEX lock(m_mutex);
    
    return m_vpnConnection.GetState();
}

HANDLE ConnectionManager::GetVPNConnectionStateChangeEvent(void)
{
    AutoMUTEX lock(m_mutex);
    
    return m_vpnConnection.GetStateChangeEvent();
}

void ConnectionManager::RemoveVPNConnection(void)
{
    AutoMUTEX lock(m_mutex);

    m_vpnConnection.Remove();
}

void ConnectionManager::VPNEstablish(void)
{
    // Kick off the VPN connection establishment

    AutoMUTEX lock(m_mutex);
    
    if (!m_vpnConnection.Establish(NarrowToTString(m_currentSessionInfo.GetServerAddress()),
                                   NarrowToTString(m_currentSessionInfo.GetPSK())))
    {
        // This is a local error, we should not try the next server because
        // we'll likely end up in an infinite loop.
        // UPDATE: was throwing Abort for reason above, now throwing TryNextServer
        // to try SSH instead. Assumes we always can try SSH, otherwise we should
        // still abort.
        // NOTE: if we know for certain that VPN won't work, we should record that
        // and stop trying here (and elsewhere in the retry loop)
        throw TryNextServer();
    }
}

void ConnectionManager::WaitForVPNConnectionStateToChangeFrom(VPNConnectionState state)
{
    // NOTE: no lock, as in ConnectionManagerStartThread

    VPNConnectionState originalState = state;

    int totalWaitMilliseconds = 0;

    while (state == GetVPNConnectionState())
    {
        HANDLE stateChangeEvent = GetVPNConnectionStateChangeEvent();

        // Wait for RasDialCallback to set a new state, or timeout (to check cancel/termination)

        // Also, wait no longer than VPN_CONNECTION_TIMEOUT_SECONDS... overriding any system
        // configuration built-in VPN client timeout (which we've found to be too long -- over a minute).

        int waitMilliseconds = 100;
        DWORD result = WaitForSingleObject(stateChangeEvent, waitMilliseconds);

        totalWaitMilliseconds += waitMilliseconds;

        if (GetUserSignalledStop() || result == WAIT_FAILED || result == WAIT_ABANDONED)
        {
            throw Abort();
        }
        else if (
            result == WAIT_TIMEOUT &&
            originalState == VPN_CONNECTION_STATE_STARTING &&
            totalWaitMilliseconds >= VPN_CONNECTION_TIMEOUT_SECONDS*1000)
        {
            throw TryNextServer();
        }
    }
}

// ==== SSH Session Functions =================================================

bool ConnectionManager::CurrentServerSSHCapable()
{
    AutoMUTEX lock(m_mutex);

    return m_currentSessionInfo.GetSSHHostKey().length() > 0;
}

tstring ConnectionManager::GetSSHConnectRequestPath(int connectType)
{
    AutoMUTEX lock(m_mutex);

    // TODO: get SSH session ID?

    return tstring(HTTP_CONNECTED_REQUEST_PATH) + 
           _T("?propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") +  (connectType == SSH_CONNECT_OBFUSCATED ? _T("OSSH") : _T("SSH")) +
           _T("&session_id=") + NarrowToTString(m_currentSessionInfo.GetSSHSessionID());
}

tstring ConnectionManager::GetSSHStatusRequestPath(int connectType, bool connected)
{
    AutoMUTEX lock(m_mutex);

    // TODO: get error code from SSH client?

    return tstring(HTTP_STATUS_REQUEST_PATH) + 
           _T("?propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") +  (connectType == SSH_CONNECT_OBFUSCATED ? _T("OSSH") : _T("SSH")) +
           _T("&session_id=") + NarrowToTString(m_currentSessionInfo.GetSSHSessionID()) +
           _T("&connected=") + (connected ? _T("1") : _T("0"));
}

tstring ConnectionManager::GetSSHFailedRequestPath(int connectType)
{
    AutoMUTEX lock(m_mutex);

    std::stringstream s;
    s << connectType;

    // TODO: get error code from SSH client?

    return tstring(HTTP_FAILED_REQUEST_PATH) + 
           _T("?propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&relay_protocol=") +  (connectType == SSH_CONNECT_OBFUSCATED ? _T("OSSH") : _T("SSH")) +
           _T("&error_code=0");
}

bool ConnectionManager::SSHConnect(int connectType)
{
    AutoMUTEX lock(m_mutex);

    return m_sshConnection.Connect(
            connectType,
            NarrowToTString(m_currentSessionInfo.GetServerAddress()),
            NarrowToTString(m_currentSessionInfo.GetSSHPort()),
            NarrowToTString(m_currentSessionInfo.GetSSHHostKey()),
            NarrowToTString(m_currentSessionInfo.GetSSHUsername()),
            NarrowToTString(m_currentSessionInfo.GetSSHPassword()),
            NarrowToTString(m_currentSessionInfo.GetSSHObfuscatedPort()),
            NarrowToTString(m_currentSessionInfo.GetSSHObfuscatedKey()),
            m_currentSessionInfo.GetPageViewRegexes(),
            m_currentSessionInfo.GetHttpsRequestRegexes());
}

void ConnectionManager::SSHDisconnect(void)
{
    // Note: no lock

    m_sshConnection.Disconnect();
}

bool ConnectionManager::SSHWaitForConnected(void)
{
    // Note: no lock

    return m_sshConnection.WaitForConnected();
}

void ConnectionManager::SSHWaitAndDisconnect(void)
{
    // Note: no lock

    m_sshConnection.WaitAndDisconnect(this);
}

void ConnectionManager::MarkCurrentServerFailed(void)
{
    AutoMUTEX lock(m_mutex);
    
    m_vpnList.MarkCurrentServerFailed();
}

// ==== General Session Functions =============================================

void ConnectionManager::LoadNextServer(
        ServerEntry& serverEntry,
        tstring& handshakeRequestPath)
{
    // Select next server to try to connect to

    AutoMUTEX lock(m_mutex);
    
    try
    {
        // Try the next server in our list.
        serverEntry = m_vpnList.GetNextServer();
    }
    catch (std::exception &ex)
    {
        my_print(false, string("LoadNextServer caught exception: ") + ex.what());
        throw Abort();
    }

    // Current session holds server entry info and will also be loaded
    // with homepage and other info.

    m_currentSessionInfo.Set(serverEntry);

    // Output values used in next TryNextServer step

    handshakeRequestPath = tstring(HTTP_HANDSHAKE_REQUEST_PATH) + 
                           _T("?propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
                           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
                           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
                           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret());

    // Include a list of known server IP addresses in the request query string as required by /handshake

    ServerEntries serverEntries =  m_vpnList.GetList();
    for (ServerEntryIterator ii = serverEntries.begin(); ii != serverEntries.end(); ++ii)
    {
        handshakeRequestPath += _T("&known_server=");
        handshakeRequestPath += NarrowToTString(ii->serverAddress);
    }
}

void ConnectionManager::HandleHandshakeResponse(const char* handshakeResponse)
{
    // Parse handshake response
    // - get PSK, which we use to connect to VPN
    // - get homepage, which we'll launch later
    // - add discovered servers to local list

    AutoMUTEX lock(m_mutex);
    
    if (!m_currentSessionInfo.ParseHandshakeResponse(handshakeResponse))
    {
        my_print(false, _T("HandleHandshakeResponse: ParseHandshakeResponse failed."));
        throw TryNextServer();
    }

    try
    {
        m_vpnList.AddEntriesToList(m_currentSessionInfo.GetDiscoveredServerEntries());
    }
    catch (std::exception &ex)
    {
        my_print(false, string("HandleHandshakeResponse caught exception: ") + ex.what());
        // This isn't fatal.  The VPN connection can still be established.
    }
}

bool ConnectionManager::RequireUpgrade(tstring& downloadRequestPath)
{
    AutoMUTEX lock(m_mutex);
    
    if (m_currentSessionInfo.GetUpgradeVersion().size() > 0)
    {
        downloadRequestPath = tstring(HTTP_DOWNLOAD_REQUEST_PATH) + 
                              _T("?propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
                              _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
                              _T("&client_version=") + NarrowToTString(m_currentSessionInfo.GetUpgradeVersion()) +
                              _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret());
        return true;
    }

    return false;
}

bool ConnectionManager::DoUpgrade(const string& download)
{
    AutoMUTEX lock(m_mutex);

    // Find current process binary path

    TCHAR filename[1000];
    if (!GetModuleFileName(NULL, filename, 1000))
    {
        // Abort upgrade: Establish() will proceed.
        return false;
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
        my_print(false, s.str().c_str());
        
        // Try to restore the original version
        if (bArchiveCreated)
        {
            CopyFile(archive_filename.c_str(), filename, FALSE);
        }

        // Abort upgrade: Establish() will proceed.
        return false;
    }

    // Don't teardown connection: see comment in VPNConnection::Remove

    m_vpnConnection.SuspendTeardownForUpgrade();

    // Die & respawn
    // TODO: if ShellExecute fails, don't die?

    ShellExecute(0, NULL, filename, 0, 0, SW_SHOWNORMAL);
    PostMessage(g_hWnd, WM_QUIT, 0, 0);

    return true;
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
            my_print(true, _T("ProcessSplitTunnelResponse failed (%d)"), ret);
            m_splitTunnelRoutes = "";
            break;
        }
        out[CHUNK_SIZE - stream.avail_out] = '\0';
        m_splitTunnelRoutes += out;
        if (m_splitTunnelRoutes.length() > SANITY_CHECK_SIZE)
        {
            my_print(true, _T("ProcessSplitTunnelResponse overflow"));
            m_splitTunnelRoutes = "";
            break;
        }
    } while (ret != Z_STREAM_END);

    inflateEnd(&stream);
}
