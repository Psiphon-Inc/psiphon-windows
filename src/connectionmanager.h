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
#include "serverlist.h"
#include "sessioninfo.h"
#include "psiclient.h"
#include "local_proxy.h"


class ITransport;


enum ConnectionManagerState
{
    CONNECTION_MANAGER_STATE_STOPPED = 0,
    CONNECTION_MANAGER_STATE_STARTING,
    CONNECTION_MANAGER_STATE_CONNECTED
};


class ConnectionManager : public ILocalProxyStatsCollector
{
public:
    ConnectionManager(void);
    virtual ~ConnectionManager(void);
    void Toggle(const tstring& transport, bool startSplitTunnel);
    void Stop(void);
    void Start(const tstring& transport, bool startSplitTunnel);
    void StartSplitTunnel();
    void StopSplitTunnel();
    time_t GetStartingTime(void);
    void SetState(ConnectionManagerState newState);
    ConnectionManagerState GetState(void);
    const bool& GetUserSignalledStop(bool throwIfTrue);
    void OpenHomePages(const TCHAR* defaultHomePage=0);
    bool SendStatusMessage(
            bool connected,
            const map<string, int>& pageViewEntries,
            const map<string, int>& httpsRequestEntries,
            unsigned long long bytesTransferred);

private:
    static DWORD WINAPI ConnectionManagerStartThread(void* object);
    static DWORD WINAPI ConnectionManager::UpgradeThread(void* object);

    // Exception classes to help with the ConnectionManagerStartThread control flow
    class Abort { };

    void DoPostConnect(const SessionInfo& sessionInfo);

    tstring GetFailedRequestPath(ITransport* transport);
    tstring GetConnectRequestPath(ITransport* transport);
    tstring GetStatusRequestPath(ITransport* transport, bool connected);
    void GetUpgradeRequestInfo(SessionInfo& sessionInfo, tstring& requestPath);

    tstring GetSpeedRequestPath(
        const tstring& relayProtocol,
        const tstring& operation,
        const tstring& info,
        DWORD milliseconds,
        DWORD size);
    void GetSpeedTestURL(tstring& serverAddress, int& serverPort, tstring& requestPath);

    void MarkCurrentServerFailed(void);
    void LoadNextServer(tstring& handshakeRequestPath);
    bool RequireUpgrade(void);
    void PaveUpgrade(const string& download);
    void ProcessSplitTunnelResponse(const string& compressedRoutes);
    tstring GetSplitTunnelingFilePath();
    bool WriteSplitTunnelRoutes(const char* routes);
    bool DeleteSplitTunnelRoutes();

    void CopyCurrentSessionInfo(SessionInfo& sessionInfo);
    void UpdateCurrentSessionInfo(const SessionInfo& sessionInfo);

private:
    HANDLE m_mutex;
    ConnectionManagerState m_state;
    ServerList m_serverList;
    bool m_userSignalledStop;
    SessionInfo m_currentSessionInfo;
    HANDLE m_thread;
    HANDLE m_upgradeThread;
    time_t m_startingTime;
    ITransport* m_transport;
    bool m_upgradePending;
    bool m_startSplitTunnel;
    string m_splitTunnelRoutes;
};
