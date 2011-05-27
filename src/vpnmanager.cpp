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
#include "vpnmanager.h"
#include "httpsrequest.h"
#include "webbrowser.h"
#include "embeddedvalues.h"
#include <algorithm>
#include <sstream>


// Upgrade process posts a Quit message
extern HWND g_hWnd;


VPNManager::VPNManager(void) :
    m_state(VPN_MANAGER_STATE_STOPPED),
    m_userSignalledStop(false),
    m_thread(0)
{
    m_mutex = CreateMutex(NULL, FALSE, 0);
}

VPNManager::~VPNManager(void)
{
    Stop();
    CloseHandle(m_mutex);
}

void VPNManager::Toggle()
{
    // NOTE: no lock, to allow thread to access object

    if (m_state == VPN_MANAGER_STATE_STOPPED)
    {
        Start();
    }
    else
    {
        Stop();
    }
}

void VPNManager::Stop(void)
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

void VPNManager::Start(void)
{
    // Call Stop to cleanup in case thread failed on last Start attempt
    Stop();

    AutoMUTEX lock(m_mutex);

    m_userSignalledStop = false;

    if (m_state != VPN_MANAGER_STATE_STOPPED || m_thread != 0)
    {
        my_print(false, _T("Invalid VPN manager state in Start (%d)"), m_state);
        return;
    }

    SetState(VPN_MANAGER_STATE_STARTING);

    if (!(m_thread = CreateThread(0, 0, VPNManagerStartThread, (void*)this, 0, 0)))
    {
        my_print(false, _T("Start: CreateThread failed (%d)"), GetLastError());

        SetState(VPN_MANAGER_STATE_STOPPED);
    }
}

DWORD WINAPI VPNManager::VPNManagerStartThread(void* data)
{
    VPNManager* manager = (VPNManager*)data;

    // Loop through server list, attempting to connect.
    //
    // Connect sequence:
    //
    // [1] Make Handshake HTTPS request
    // [1a] Perform download HTTPS request and upgrade, if applicable
    // [2] Create and dial VPN connection
    // [3] Wait for VPN connection to succeed or fail
    // [4] Launch home pages (failure is acceptable)
    // [5] Make Connected HTTPS request (failure is acceptable)
    // [6] Wait for VPN connection to stop
    //
    // When any of 1-3 fail, the server is marked as failed
    // in the local server list and the next server from the
    // list is selected and retried.
    //
    // All operations may be interrupted by user cancel.
    //
    // NOTE: this function doesn't hold the VPNManager
    // object lock to allow for cancel etc.

    while (true) // Try servers loop
    {
        try
        {
            //
            // [1] Handshake HTTPS request
            //

            tstring serverAddress;
            int webPort;
            string webServerCertificate;
            tstring handshakeRequestPath;
            string handshakeResponse;

            manager->LoadNextServer(
                            serverAddress,
                            webPort,
                            webServerCertificate,
                            handshakeRequestPath);

            HTTPSRequest httpsRequest;
            if (!httpsRequest.GetRequest(
                                manager->GetUserSignalledStop(),
                                serverAddress.c_str(),
                                webPort,
                                webServerCertificate,
                                handshakeRequestPath.c_str(),
                                handshakeResponse))
            {
                if (manager->GetUserSignalledStop())
                {
                    throw Abort();
                }
                else
                {
                    throw TryNextServer();
                }
            }

            manager->HandleHandshakeResponse(handshakeResponse.c_str());

            //
            // [1a] Upgrade
            //

            // Upgrade now if handshake notified of new version
            tstring downloadRequestPath;
            string downloadResponse;
            if (manager->RequireUpgrade(downloadRequestPath))
            {
                // Download new binary

                if (!httpsRequest.GetRequest(
                            manager->GetUserSignalledStop(),
                            serverAddress.c_str(),
                            webPort,
                            webServerCertificate,
                            downloadRequestPath.c_str(),
                            downloadResponse))
                {
                    if (manager->GetUserSignalledStop())
                    {
                        throw Abort();
                    }
                    else
                    {
                        // TODO: Proceed with the connection if the download fails?
                        throw TryNextServer();
                    }
                }

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

                // Fall through to Establish()
            }

            //
            // [2] Start VPN connection
            //

            manager->Establish();

            //
            // [3] Monitor VPN connection and wait for CONNECTED or FAILED
            //

            manager->WaitForVPNConnectionStateToChangeFrom(VPN_CONNECTION_STATE_STARTING);

            if (VPN_CONNECTION_STATE_CONNECTED != manager->GetVPNConnectionState())
            {
                throw TryNextServer();
            }

            manager->SetState(VPN_MANAGER_STATE_CONNECTED);

            //
            // [4] Open home pages in browser
            //

            manager->OpenHomePages();

            //
            // [5] "Connected" HTTPS request for server stats (not critical to succeed)
            //

            // There's no content in the response. Also, failure is ignored since
            // it just means the server didn't log a stat.

            tstring connectedRequestPath = manager->GetConnectRequestPath();
        
            string response;
            if (!httpsRequest.GetRequest(
                                manager->GetUserSignalledStop(),
                                serverAddress.c_str(),
                                webPort,
                                webServerCertificate,
                                connectedRequestPath.c_str(),
                                response))
            {
                // Ignore failure
            }

            //
            // [6] Wait for VPN connection to stop (or fail) -- set VPNManager state accordingly (used by UI)
            //

            manager->WaitForVPNConnectionStateToChangeFrom(VPN_CONNECTION_STATE_CONNECTED);

            manager->SetState(VPN_MANAGER_STATE_STOPPED);

            break;
        }
        catch (Abort&)
        {
            manager->RemoveVPNConnection();
            manager->SetState(VPN_MANAGER_STATE_STOPPED);
            break;
        }
        catch (TryNextServer&)
        {
            manager->RemoveVPNConnection();
            manager->MarkCurrentServerFailed();
            // Continue while loop to try next server
        }
    }

    return 0;
}

void VPNManager::MarkCurrentServerFailed(void)
{
    AutoMUTEX lock(m_mutex);
    
    m_vpnList.MarkCurrentServerFailed();
}

VPNConnectionState VPNManager::GetVPNConnectionState(void)
{
    AutoMUTEX lock(m_mutex);
    
    return m_vpnConnection.GetState();
}

HANDLE VPNManager::GetVPNConnectionStateChangeEvent(void)
{
    AutoMUTEX lock(m_mutex);
    
    return m_vpnConnection.GetStateChangeEvent();
}

void VPNManager::RemoveVPNConnection(void)
{
    AutoMUTEX lock(m_mutex);

    m_vpnConnection.Remove();
}

void VPNManager::OpenHomePages(void)
{
    AutoMUTEX lock(m_mutex);
    
    OpenBrowser(m_currentSessionInfo.GetHomepages());
}

tstring VPNManager::GetConnectRequestPath(void)
{
    AutoMUTEX lock(m_mutex);

    return tstring(HTTP_CONNECTED_REQUEST_PATH) + 
           _T("?client_id=") + NarrowToTString(CLIENT_ID) +
           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret()) +
           _T("&vpn_client_ip_address=") + m_vpnConnection.GetPPPIPAddress();
}

void VPNManager::LoadNextServer(
        tstring& serverAddress,
        int& webPort,
        string& serverCertificate,
        tstring& handshakeRequestPath)
{
    // Select next server to try to connect to

    AutoMUTEX lock(m_mutex);
    
    ServerEntry serverEntry;

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

    serverAddress = NarrowToTString(serverEntry.serverAddress);
    webPort = serverEntry.webServerPort;
    serverCertificate = serverEntry.webServerCertificate;
    handshakeRequestPath = tstring(HTTP_HANDSHAKE_REQUEST_PATH) + 
                           _T("?client_id=") + NarrowToTString(CLIENT_ID) +
                           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
                           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
                           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret());
}

void VPNManager::HandleHandshakeResponse(const char* handshakeResponse)
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

void VPNManager::Establish(void)
{
    // Kick off the VPN connection establishment

    AutoMUTEX lock(m_mutex);
    
    if (!m_vpnConnection.Establish(NarrowToTString(m_currentSessionInfo.GetServerAddress()),
                                   NarrowToTString(m_currentSessionInfo.GetPSK())))
    {
        // This is a local error, we should not try the next server because
        // we'll likely end up in an infinite loop.
        throw Abort();
    }
}

void VPNManager::WaitForVPNConnectionStateToChangeFrom(VPNConnectionState state)
{
    AutoMUTEX lock(m_mutex);

    while (VPN_CONNECTION_STATE_STARTING == GetVPNConnectionState())
    {
        HANDLE stateChangeEvent = GetVPNConnectionStateChangeEvent();

        // Wait for RasDialCallback to set a new state, or timeout (to check cancel/termination)
        DWORD result = WaitForSingleObject(stateChangeEvent, 100);

        if (GetUserSignalledStop() || result == WAIT_FAILED || result == WAIT_ABANDONED)
        {
            throw Abort();
        }
    }
}
    
bool VPNManager::RequireUpgrade(tstring& downloadRequestPath)
{
    AutoMUTEX lock(m_mutex);
    
    if (m_currentSessionInfo.GetUpgradeVersion().size() > 0)
    {
        downloadRequestPath = tstring(HTTP_DOWNLOAD_REQUEST_PATH) + 
                              _T("?client_id=") + NarrowToTString(CLIENT_ID) +
                              _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
                              _T("&client_version=") + NarrowToTString(m_currentSessionInfo.GetUpgradeVersion()) +
                              _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret());
        return true;
    }

    return false;
}

bool VPNManager::DoUpgrade(const string& download)
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
