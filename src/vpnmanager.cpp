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
    AutoMUTEX lock(m_mutex);

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

    // Cancel flag is also termination flag
    m_userSignalledStop = true;

    // Wait for TryNextServer thread to exit (otherwise can get access violation when app terminates)
    if (m_thread)
    {
        WaitForSingleObject(m_thread, 10000);
        m_thread = 0;
    }

    m_vpnConnection.Remove();
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
    //
    // When any of 1-3 fail, the server is marked as failed
    // in the local server list and the next server from the
    // list is selected and retried.
    //
    // All operations may be interrupted by user cancel.
    //
    // NOTE: this function doesn't hold the VPNManager
    // object lock to allow for cancel etc.

    while(true)
    {
        //
        // [1] Handshake HTTPS request
        //

        tstring serverAddress;
        int webPort;
        string webServerCertificate;
        tstring handshakeRequestPath;
        string handshakeResponse;

        if (!manager->LoadNextServer(
                            serverAddress,
                            webPort,
                            webServerCertificate,
                            handshakeRequestPath))
        {
            // No retry here, it's an internal error

            manager->SetState(VPN_MANAGER_STATE_STOPPED);
            return 0;
        }

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
                manager->SetState(VPN_MANAGER_STATE_STOPPED);
                return 0;
            }

            // Retry next server

            manager->MarkCurrentServerFailed();
            continue;
        }

        if (!manager->HandleHandshakeResponse(handshakeResponse.c_str()))
        {
            // No retry here, it's an internal error
            // (TODO: or corrupt server data, so retry...?)

            manager->SetState(VPN_MANAGER_STATE_STOPPED);
            return 0;
        }

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
                    manager->SetState(VPN_MANAGER_STATE_STOPPED);
                    return 0;
                }

                // Retry next server

                manager->MarkCurrentServerFailed();
                continue;
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

        if (!manager->Establish())
        {
            if (manager->GetUserSignalledStop())
            {
                manager->SetState(VPN_MANAGER_STATE_STOPPED);
                return 0;
            }

            // Retry next server

            manager->MarkCurrentServerFailed();
            continue;
        }

        //
        // [3] Monitor VPN connection and wait for CONNECTED or FAILED
        //

        bool retry = false;

        while (true)
        {
            HANDLE stateChangeEvent = manager->GetVPNConnectionStateChangeEvent();

            // Wait for RasDialCallback to set a new state, or timeout (to check cancel/termination)
            DWORD result = WaitForSingleObject(stateChangeEvent, 100);

            if (result == WAIT_TIMEOUT)
            {
                if (manager->GetUserSignalledStop())
                {
                    manager->RemoveVPNConnection();
                    manager->SetState(VPN_MANAGER_STATE_STOPPED);
                    return 0;
                }
            }
            else if (result != WAIT_OBJECT_0)
            {
                // internal error
                manager->RemoveVPNConnection();
                manager->SetState(VPN_MANAGER_STATE_STOPPED);
                return 0;
            }
            else
            {
                VPNConnectionState state = manager->GetVPNConnectionState();

                if (state == VPN_CONNECTION_STATE_CONNECTED)
                {
                    // Go on to next step
                    break;
                }
                else if (state != VPN_CONNECTION_STATE_STARTING)
                {
                    // FAILED or STOPPED: Retry next server

                    retry = true;
                    manager->MarkCurrentServerFailed();
                    break;
                }
            }
        }

        if (retry)
        {
            continue;
        }

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

        manager->SetState(VPN_MANAGER_STATE_CONNECTED);
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
           _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret());
}

bool VPNManager::LoadNextServer(
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
        return false;
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

    return true;
}

bool VPNManager::HandleHandshakeResponse(const char* handshakeResponse)
{
    // Parse handshake response
    // - get PSK, which we use to connect to VPN
    // - get homepage, which we'll launch later
    // - add discovered servers to local list

    AutoMUTEX lock(m_mutex);
    
    if (!m_currentSessionInfo.ParseHandshakeResponse(handshakeResponse))
    {
        return false;
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

    return true;
}

bool VPNManager::Establish(void)
{
    // Kick off the VPN connection establishment

    AutoMUTEX lock(m_mutex);
    
    if (!m_vpnConnection.Establish(NarrowToTString(m_currentSessionInfo.GetServerAddress()),
                                   NarrowToTString(m_currentSessionInfo.GetPSK())))
    {
        return false;
    }

    return true;
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
