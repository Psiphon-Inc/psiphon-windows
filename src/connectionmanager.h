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
#include "sessioninfo.h"
#include "psiclient.h"
#include "local_proxy.h"
#include "transport.h"


class ITransport;


enum ConnectionManagerState
{
    CONNECTION_MANAGER_STATE_STOPPED = 0,
    CONNECTION_MANAGER_STATE_STARTING,
    CONNECTION_MANAGER_STATE_CONNECTED,
    CONNECTION_MANAGER_STATE_STOPPING
};


class ConnectionManager : public ILocalProxyStatsCollector, public IReconnectStateReceiver
{
public:
    ConnectionManager();
    virtual ~ConnectionManager();

    void Toggle();
    void Stop(DWORD reason);
    void Start();
    void SetState(ConnectionManagerState newState);
    ConnectionManagerState GetState();
    void OpenHomePages(const TCHAR* defaultHomePage=0, bool allowSkip=true);

    // IReconnectStateReceiver implementation
    virtual void SetReconnecting();
    virtual void SetReconnected();

    // ILocalProxyStatsCollector implementation
    // May throw StopSignal::StopException subclass if not `final`
    virtual bool SendStatusMessage(
            bool final,
            const map<string, int>& pageViewEntries,
            const map<string, int>& httpsRequestEntries,
            unsigned long long bytesTransferred);

    // Results in WM_PSIPHON_FEEDBACK_SUCCESS being posted to the main window
    // on success, WM_PSIPHON_FEEDBACK_FAILED on failure.
    // NOTE: The JSON string must contain wide unicode codepoints, not UTF-8.
    void SendFeedback(LPCWSTR unicodeFeedbackJSON);

    bool IsWholeSystemTunneled()
    {
        return m_transport
            && m_transport->IsWholeSystemTunneled()
            && m_transport->IsConnected(true);
    }

private:
    static DWORD WINAPI ConnectionManagerStartThread(void* object);
    static DWORD WINAPI ConnectionManagerUpgradeThread(void* object);

    // Exception classes to help with the ConnectionManagerStartThread control flow
    class Abort { };

    void DoPostConnect(const SessionInfo& sessionInfo, bool openHomePages);

    tstring GetFailedRequestPath(ITransport* transport);
    tstring GetConnectRequestPath(ITransport* transport);
    // May return empty string, which indicates that status can't be sent.
    tstring GetStatusRequestPath(ITransport* transport, bool connected);
    void GetUpgradeRequestInfo(SessionInfo& sessionInfo, tstring& requestPath);

    void FetchRemoteServerList();

    bool RequireUpgrade();
    void PaveUpgrade(const string& download);

    void CopyCurrentSessionInfo(SessionInfo& sessionInfo);
    void UpdateCurrentSessionInfo(const SessionInfo& sessionInfo);

    // May throw StopSignal::StopException
    bool DoSendFeedback(LPCWSTR feedbackJSON);
    static DWORD WINAPI ConnectionManagerFeedbackThread(void* object);

private:
    HANDLE m_mutex;
    ConnectionManagerState m_state;
    SessionInfo m_currentSessionInfo;
    HANDLE m_thread;
    HANDLE m_upgradeThread;
    HANDLE m_feedbackThread;
    ITransport* m_transport;
    bool m_upgradePending;
    bool m_startSplitTunnel;
    time_t m_nextFetchRemoteServerListAttempt;
};
