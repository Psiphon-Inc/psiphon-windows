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
    m_vpnState(VPN_STATE_STOPPED),
    m_userSignalledStop(false),
    m_tryNextServerThreadHandle(0)
{
    m_mutex = CreateMutex(NULL, FALSE, 0);
}

VPNManager::~VPNManager(void)
{
    Stop();

    // Wait for TryNextServer thread to exit (otherwise can get access violation when app terminates)
    if (m_tryNextServerThreadHandle)
    {
        WaitForSingleObject(m_tryNextServerThreadHandle, 10000);
        m_tryNextServerThreadHandle = 0;
    }

    CloseHandle(m_mutex);
}

void VPNManager::Toggle()
{
    AutoMUTEX lock(m_mutex);

    switch (m_vpnState)
    {
    case VPN_STATE_STOPPED:
        // The user clicked the button to start the VPN.
        // Clear this flag so we can do retries on failed connections.
        m_userSignalledStop = false;
        TryNextServer();
        break;

    default:
        // The user requested to stop the VPN by clicking the button.
        //
        // If a connection was in the INITIALIZING state, this flag
        // tells TryNextServer not to Establish, or to Stop if
        // Establish was already called.
        // NOTE that Stop is called here in case TryNextServer has
        // already returned (and the first callback notification has
        // not yet been received).
        //
        // If a connection was in the STARTING state, we will get a
        // "Connection Failed" notification.
        // This flag indicates that we should not retry when a failed
        // connection is signalled.
        m_userSignalledStop = true;
        Stop();
        break;
    }
}

void VPNManager::Stop(void)
{
    AutoMUTEX lock(m_mutex);

    if (m_vpnConnection.Remove())
    {
        // TODO:
        //
        // This call was here for some now-forgotten reason: restore a known state or something.
        // However, it was causing a problem: after this STOPPED state was set, the user was able
        // to start a new Establish, but the RasCallback that was previously cancelled kicked in
        // after that Establish with a FAILED state change that caused a retry, which causes
        // VPNStateChanged to TryNextServer... end up with two simultaneous connection threads.
        //
        // Also:
        //
        // Another potential problem is that Stop() is invoked -- from a user Toggle -- after
        // Establish() but before the RasCallback thread is invoked, and RasHangup simply
        // succeeds (ERROR_NO_CONNECTION).  In this case, we go into the STOPPED state while
        // a connection is still outstanding.  Again, leading to multiple simultanetous connection
        // threads.  Maybe.
        //
        //VPNStateChanged(VPN_STATE_STOPPED);
    }
}

void VPNManager::VPNStateChanged(VPNState newState)
{
    AutoMUTEX lock(m_mutex);

    m_vpnState = newState;

    switch (m_vpnState)
    {
    case VPN_STATE_CONNECTED:
        OpenBrowser(m_currentSessionInfo.GetHomepages());

        {
        // !!!
        // TODO: this request will block the caller, but need to rework the whole architecture of this code anyway
        // !!!

        // Make "/connected" request, which the server logs to generate
        // successful-connect-to-VPN stats.
        // There's no content in the response. Also, failure is ignored since
        // it just means the server didn't log a stat.
        tstring connectedRequestPath = tstring(HTTP_CONNECTED_REQUEST_PATH) + 
                                       _T("?client_id=") + NarrowToTString(CLIENT_ID) +
                                       _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
                                       _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
                                       _T("&server_secret=") + NarrowToTString(m_currentSessionInfo.GetWebServerSecret());
        HTTPSRequest httpsRequest;
        string response;
        httpsRequest.GetRequest(
                            m_userSignalledStop,
                            NarrowToTString(m_currentSessionInfo.GetServerAddress()).c_str(),
                            m_currentSessionInfo.GetWebPort(),
                            m_currentSessionInfo.GetWebServerCertificate().c_str(),
                            connectedRequestPath.c_str(),
                            response);
        }
        break;

    case VPN_STATE_FAILED:
        // Either the user cancelled an in-progress connection,
        // or a connection actually failed.
        // Either way, we need to set the status to STOPPED,
        // so that another Toggle() will cause the VPN to start again.
        if (m_userSignalledStop)
        {
            m_vpnState = VPN_STATE_STOPPED;
        }
        else
        {
            // Connecting to the current server failed.
            try
            {
                m_vpnList.MarkCurrentServerFailed();
                // WARNING: Don't try to optimize this code.
                // It is important for the state not to become STOPPED before TryNextServer
                // is called, to ensure that a button click does not invoke a second concurrent
                // TryNextServer.
                TryNextServer();
            }
            catch (std::exception &ex)
            {
                my_print(false, string("VPNStateChanged caught exception: ") + ex.what());
                m_vpnState = VPN_STATE_STOPPED;
            }
        }
        break;

    default:
        // no default actions
        break;
    }
}

void VPNManager::TryNextServer(void)
{
    AutoMUTEX lock(m_mutex);

    // The INITIALIZING state is set here, instead of inside the thread, to prevent a second
    // button click from triggering a second concurrent TryNextServer invocation.
    VPNStateChanged(VPN_STATE_INITIALIZING);

    // This function might not return quickly, because it performs an HTTPS Request.
    // It is run in a thread so that it does not starve the message pump.
    m_tryNextServerThreadHandle = CreateThread(0, 0, TryNextServerThread, (void*)this, 0, 0);

    if (!m_tryNextServerThreadHandle)
    {
        my_print(false, _T("TryNextServer: CreateThread failed (%d)"), GetLastError());
        VPNStateChanged(VPN_STATE_STOPPED);
    }
}

DWORD WINAPI VPNManager::TryNextServerThread(void* data)
{
    // By design, this function doesn't hold the VPNManager lock for the
    // duration. This allows the user to cancel the operation.
    //
    // This is all a stupid mess and a proper state machine should be
    // introduced here.  But it works well enough for now.
    //
    // If the user clicks cancel before Establish() is invoked, this thread
    // will change the state from INITIALIZING --> STOPPED. A second button
    // click before that transition should have no effect.
    //
    // If any of LoadNextServer, DoHandshake, Establish, HandleHandshakeResponse
    // fail, they will transition the state to STOPPED or FAILED.
    // In the case of failed, the a 2nd thread will start before this one
    // exits, and the state will go back to INITIALIZED and bypass STOPPED so
    // user button clicks won't start a new connection thread.
    //
    // When the state goes to STOPPED, another button click can start a new
    // connecttion attempt thread.  So in all cases, once the state becomes
    // STOPPED, this thread must immediately exit without modifying the
    // VPN state.

    VPNManager* manager = (VPNManager*)data;

    tstring serverAddress;
    int webPort;
    string webServerCertificate;
    tstring handshakeRequestPath;
    string handshakeResponse;

    if (!manager->LoadNextServer(serverAddress, webPort, webServerCertificate,
                                 handshakeRequestPath))
    {
        // Helper function sets state to STOPPED or FAILED
        return 0;
    }

    // NOTE: DoHandshake doesn't hold the VPNManager lock for the entire
    // web request transaction.

    if (!manager->DoHandshake(serverAddress.c_str(), webPort, webServerCertificate,
                              handshakeRequestPath.c_str(), handshakeResponse))
    {
        // Helper function sets state to STOPPED or FAILED
        return 0;
    }

    if (!manager->HandleHandshakeResponse(handshakeResponse.c_str()))
    {
        // Helper function sets state to STOPPED or FAILED
        return 0;
    }

    // Abort now if user clicked cancel during web request
    if (manager->GetUserSignalledStop())
    {
        manager->VPNStateChanged(VPN_STATE_STOPPED);
        return 0;
    }

    // Upgrade now if handshake notified of new version
    tstring downloadRequestPath;
    string downloadResponse;
    if (manager->RequireUpgrade(downloadRequestPath))
    {
        // Download new binary

        if (!manager->DoDownload(serverAddress.c_str(), webPort, webServerCertificate,
                                 downloadRequestPath.c_str(), downloadResponse))
        {
            // Helper function sets state to STOPPED or FAILED
            return 0;
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

    if (!manager->Establish())
    {
        // Helper function sets state to STOPPED or FAILED
        return 0;
    }

    return 0;
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

        // NOTE: state change assumes we're calling LoadNextServer in sequence in TryNextServer thread
        VPNStateChanged(VPN_STATE_STOPPED);
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

bool VPNManager::DoHandshake(
        const TCHAR* serverAddress,
        int webPort,
        const string& webServerCertificate,
        const TCHAR* handshakeRequestPath,
        string& handshakeResponse)
{
    // Perform handshake HTTPS request

    // NOTE: Lock isn't held while making network request
    //       Ensure AutoMUTEX() is created unless calling
    //       synchronized member function.
    //
    //       An assumption is made that other code won't
    //       change the state value while the HTTP request
    //       is performed and the VPNManager is unlocked.

    HTTPSRequest httpsRequest;
    if (!httpsRequest.GetRequest(
                        m_userSignalledStop,
                        serverAddress,
                        webPort,
                        webServerCertificate,
                        handshakeRequestPath,
                        handshakeResponse))
    {
        // NOTE: state change assumes we're calling DoHandshake in sequence in TryNextServer thread
        if (m_userSignalledStop)
        {
            VPNStateChanged(VPN_STATE_STOPPED);
        }
        else
        {
            VPNStateChanged(VPN_STATE_FAILED);
        }
        return false;
    }

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
        // NOTE: state change assumes we're calling DoHandshake in sequence in TryNextServer thread
        VPNStateChanged(VPN_STATE_FAILED);
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
        // NOTE: state change assumes we're calling Establish in sequence in TryNextServer thread
        VPNStateChanged(VPN_STATE_STOPPED);
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

bool VPNManager::DoDownload(
        const TCHAR* serverAddress,
        int webPort,
        const string& webServerCertificate,
        const TCHAR* downloadRequestPath,
        string& downloadResponse)
{
    // Perform download HTTPS request

    // NOTE: See comment in DoHandshake regarding locking

    HTTPSRequest httpsRequest;
    if (!httpsRequest.GetRequest(
                        m_userSignalledStop,
                        serverAddress,
                        webPort,
                        webServerCertificate,
                        downloadRequestPath,
                        downloadResponse))
    {
        // NOTE: state change assumes we're calling DoDownload in sequence in TryNextServer thread
        if (m_userSignalledStop)
        {
            VPNStateChanged(VPN_STATE_STOPPED);
        }
        else
        {
            VPNStateChanged(VPN_STATE_FAILED);
        }
        return false;
    }

    return true;

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
