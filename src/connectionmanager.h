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
#include "sshconnection.h"
#include "vpnlist.h"
#include "sessioninfo.h"
#include "psiclient.h"


enum ConnectionManagerState
{
    CONNECTION_MANAGER_STATE_STOPPED = 0,
    CONNECTION_MANAGER_STATE_STARTING,
    CONNECTION_MANAGER_STATE_CONNECTED_VPN,
    CONNECTION_MANAGER_STATE_CONNECTED_SSH
};


class ConnectionManager
{
public:
    ConnectionManager(void);
    virtual ~ConnectionManager(void);
    void Toggle(void);
    void Stop(void);
    void SetState(ConnectionManagerState newState) {m_state = newState;}
    ConnectionManagerState GetState(void) {return m_state;}
    const bool& GetUserSignalledStop(void) {return m_userSignalledStop;}
    void OpenHomePages(void);

private:
    void Start(void);
    static DWORD WINAPI ConnectionManagerStartThread(void* object);
    static void DoVPNConnection(
        ConnectionManager* manager,
        const ServerEntry& serverEntry);
    static void DoSSHConnection(
        ConnectionManager* manager);

    // Exception classes to help with the ConnectionManagerStartThread control flow
    class TryNextServer { };
    class Abort { };

    void MarkCurrentServerFailed(void);
    tstring GetConnectRequestPath(void);
    tstring GetFailedRequestPath(void);
    void LoadNextServer(
        ServerEntry& serverEntry,
        tstring& handshakeRequestPath);
    void HandleHandshakeResponse(
        const char* handshakeResponse);
    bool RequireUpgrade(tstring& downloadRequestPath);
    bool DoUpgrade(const string& download);

    bool CurrentServerVPNCapable(void);
    VPNConnectionState GetVPNConnectionState(void);
    HANDLE GetVPNConnectionStateChangeEvent(void);
    void RemoveVPNConnection(void);
    void VPNEstablish(void);
    void WaitForVPNConnectionStateToChangeFrom(VPNConnectionState state);

    bool CurrentServerSSHCapable(void);
    bool SSHConnect(void);
    void SSHDisconnect(void);
    bool SSHWaitForConnected(void);
    void SSHWaitAndDisconnect(void);

    HANDLE m_mutex;
    ConnectionManagerState m_state;
    VPNList m_vpnList;
    VPNConnection m_vpnConnection;
    bool m_userSignalledStop;
    SessionInfo m_currentSessionInfo;
    SSHConnection m_sshConnection;
    HANDLE m_thread;
};
