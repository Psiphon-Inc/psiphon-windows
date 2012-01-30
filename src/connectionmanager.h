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

#include <time.h>
#include "vpnlist.h"
#include "sessioninfo.h"
#include "psiclient.h"


class TransportBase;

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
    void Start(void);
    time_t GetStartingTime(void);
    void SetState(ConnectionManagerState newState);
    ConnectionManagerState GetState(void);
    const bool& GetUserSignalledStop(void) {return m_userSignalledStop;}
    void OpenHomePages(void);
    void SetSkipVPN(void);
    void ResetSkipVPN(void);
    bool GetSkipVPN(void);
    void SetCurrentConnectionSkippedVPN(bool skippedVPN) {m_currentSessionSkippedVPN = skippedVPN;}
    bool CurrentSessionSkippedVPN(void) {return m_currentSessionSkippedVPN;}
    bool SendStatusMessage(
            bool connected,
            const map<string, int>& pageViewEntries,
            const map<string, int>& httpsRequestEntries,
            unsigned long long bytesTransferred);

private:
    static DWORD WINAPI ConnectionManagerStartThread(void* object);

    // Exception classes to help with the ConnectionManagerStartThread control flow
    class TryNextServer { };
    class Abort { };

    tstring GetSpeedRequestPath(
        const tstring& relayProtocol,
        const tstring& operation,
        const tstring& info,
        DWORD milliseconds,
        DWORD size);
    void GetSpeedTestURL(tstring& serverAddress, tstring& serverPort, tstring& requestPath);

    void MarkCurrentServerFailed(void);
    void LoadNextServer(
        ServerEntry& serverEntry,
        tstring& handshakeRequestPath);
    void HandleHandshakeResponse(
        const char* handshakeResponse);
    bool RequireUpgrade(tstring& downloadRequestPath);
    bool DoUpgrade(const string& download);
    void ProcessSplitTunnelResponse(const string& compressedRoutes);

    tstring GetFailedRequestPath(TransportBase* transport);
    tstring GetConnectRequestPath(TransportBase* transport);
    tstring GetStatusRequestPath(TransportBase* transport, bool connected);

    bool CurrentServerSSHCapable(void);
    bool SSHConnect(int connectType);
    void SSHDisconnect(void);
    bool SSHWaitForConnected(void);
    void SSHWaitAndDisconnect(void);

    void DoPostConnect();

    HANDLE m_mutex;
    ConnectionManagerState m_state;
    VPNList m_vpnList;
    bool m_userSignalledStop;
    SessionInfo m_currentSessionInfo;
    HANDLE m_thread;
    bool m_currentSessionSkippedVPN;
    time_t m_startingTime;
    string m_splitTunnelRoutes;

    // TEMP: Replace with array and accessors
public:
    TransportBase* m_currentTransport;
    TransportBase* m_vpnTransport;
    TransportBase* m_sshTransport;
    TransportBase* m_osshTransport;
};
