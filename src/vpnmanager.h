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

#pragma once

#include "vpnconnection.h"
#include "vpnlist.h"
#include "sessioninfo.h"
#include "psiclient.h"


enum VPNState
{
    VPN_STATE_STOPPED = 0,
    VPN_STATE_INITIALIZING,
    VPN_STATE_STARTING,
    VPN_STATE_CONNECTED,
    VPN_STATE_FAILED
};


class VPNManager
{
public:
    VPNManager(void);
    virtual ~VPNManager(void);
    void Toggle(void);
    void Stop(void);
    void VPNStateChanged(VPNState newState);

    VPNState GetVPNState(void)
        {AutoMUTEX lock(m_mutex); return m_vpnState;}

    bool GetUserSignalledStop(void)
        {AutoMUTEX lock(m_mutex); return m_userSignalledStop;}

private:
    void TryNextServer(void);
    static DWORD WINAPI TryNextServerThread(void* object);

    // NOTE: LoadNextServer, DoHandshake, Establish, HandleHandshakeResponse
    // are only to be called from TryNextServerThread.
    bool LoadNextServer(
        tstring& serverAddress,
        int& webPort,
        string& webServerCertificate,
        tstring& handshakeRequestPath);
    bool DoHandshake(
        const TCHAR* serverAddress,
        int webPort,
        const string& webServerCertificate,
        const TCHAR* handshakeRequestPath,
        string& handshakeResponse);
    bool HandleHandshakeResponse(
        const char* handshakeResponse);
    bool RequireUpgrade(tstring& downloadRequestPath);
    bool DoDownload(
        const TCHAR* serverAddress,
        int webPort,
        const string& webServerCertificate,
        const TCHAR* downloadRequestPath,
        string& downloadResponse);
    bool DoUpgrade(const string& download);
    bool Establish(void);

    HANDLE m_mutex;
    VPNList m_vpnList;
    VPNConnection m_vpnConnection;
    VPNState m_vpnState;
    bool m_userSignalledStop;
    SessionInfo m_currentSessionInfo;
    HANDLE m_tryNextServerThreadHandle;
};
