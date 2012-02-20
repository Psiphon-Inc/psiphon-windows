/*
 * Copyright (c) 2012, Psiphon Inc.
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

#include "worker_thread.h"

class SessionInfo;
struct RegexReplace;
class SystemProxySettings;


class ILocalProxyStatsCollector
{
public:
    virtual bool SendStatusMessage(
                    bool connected,
                    const map<string, int>& pageViewEntries,
                    const map<string, int>& httpsRequestEntries,
                    unsigned long long bytesTransferred) = 0;
};


class LocalProxy : public IWorkerThread
{
public:
    // If statsCollector is null, no stats will be collected. (This should only
    // be the case for temporary connections.)
    LocalProxy(
        ILocalProxyStatsCollector* statsCollector, 
        const SessionInfo& sessionInfo, 
        SystemProxySettings* systemProxySettings,
        int parentPort, 
        const tstring& splitTunnelingFilePath);
    virtual ~LocalProxy();

    // Sometimes SessionInfo gets updated after the LocalProxy starts (i.e.,
    // a handshake happens afterwards, with new page view regexes); in that 
    // case we need to update the SessionInfo here.
    void UpdateSessionInfo(const SessionInfo& sessionInfo);

protected:
    // IWorkerThread implementation
    bool DoStart();
    bool DoPeriodicCheck();
    void DoStop();

    void Cleanup();

    bool StartPolipo();
    bool CreatePolipoPipe(HANDLE& o_outputPipe, HANDLE& o_errorPipe);
    bool ProcessStatsAndStatus(bool connected);
    void UpsertPageView(const string& entry);
    void UpsertHttpsRequest(string entry);
    void ParsePolipoStatsBuffer(const char* page_view_buffer);

private:
    ILocalProxyStatsCollector* m_statsCollector;
    int m_parentPort;
    tstring m_polipoPath;
    tstring m_splitTunnelingFilePath;
    SystemProxySettings* m_systemProxySettings;
    PROCESS_INFORMATION m_polipoProcessInfo;
    HANDLE m_polipoPipe;
    DWORD m_lastStatusSendTimeMS;
    map<string, int> m_pageViewEntries;
    map<string, int> m_httpsRequestEntries;
    unsigned long long m_bytesTransferred;
    vector<RegexReplace> m_pageViewRegexes;
    vector<RegexReplace> m_httpsRequestRegexes;
};

