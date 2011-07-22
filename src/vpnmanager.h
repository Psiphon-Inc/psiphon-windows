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


enum VPNManagerState
{
    VPN_MANAGER_STATE_STOPPED = 0,
    VPN_MANAGER_STATE_STARTING,
    VPN_MANAGER_STATE_CONNECTED
};


class VPNManager
{
public:
    VPNManager(void);
    virtual ~VPNManager(void);
    void Toggle(void);
    void Stop(void);
    void SetState(VPNManagerState newState) {m_state = newState;}
    VPNManagerState GetState(void) {return m_state;}
    const bool& GetUserSignalledStop(void) {return m_userSignalledStop;}
    void TweakVPN();
    void TweakDNS();
    void OpenHomePages(void);

private:
    void Start(void);
    static DWORD WINAPI VPNManagerStartThread(void* object);

    // Exception classes to help with the VPNManagerStartThread control flow
    class TryNextServer { };
    class Abort { };

    void MarkCurrentServerFailed(void);
    VPNConnectionState GetVPNConnectionState(void);
    HANDLE GetVPNConnectionStateChangeEvent(void);
    void RemoveVPNConnection(void);
    tstring GetConnectRequestPath(void);
    tstring GetFailedRequestPath(void);
    void LoadNextServer(
        tstring& serverAddress,
        int& webPort,
        string& webServerCertificate,
        tstring& handshakeRequestPath);
    void HandleHandshakeResponse(
        const char* handshakeResponse);
    bool RequireUpgrade(tstring& downloadRequestPath);
    bool DoUpgrade(const string& download);
    void Establish(void);
    void WaitForVPNConnectionStateToChangeFrom(VPNConnectionState state);

    HANDLE m_mutex;
    VPNManagerState m_state;
    VPNList m_vpnList;
    VPNConnection m_vpnConnection;
    bool m_userSignalledStop;
    SessionInfo m_currentSessionInfo;
    HANDLE m_thread;
};
