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

#include "tstring.h"
#include "systemproxysettings.h"

class ConnectionManager;

enum
{
    SSH_CONNECT_OBFUSCATED = 0,
    SSH_CONNECT_STANDARD
};

class SSHConnection
{
public:
    SSHConnection(const bool& cancel);
    virtual ~SSHConnection(void);

    // TODO: async connect, state change notifiers, connection monitor, etc.?

    bool Connect(
        int connectType,
        const tstring& sshServerAddress,
        const tstring& sshServerPort,
        const tstring& sshServerHostKey,
        const tstring& sshUsername,
        const tstring& sshPassword,
        const tstring& sshObfuscatedPort,
        const tstring& sshObfuscatedKey);
    void Disconnect(void);
    bool WaitForConnected(void);
    void WaitAndDisconnect(ConnectionManager* connectionManager);
    void SignalDisconnect(void);

private:
    HANDLE CreatePolipoPipe();
    bool ProcessPageViews(unsigned int totalTenthsSeconds);
    void ParsePageViewBuffer(const char* page_view_buffer);

private:
    SystemProxySettings m_systemProxySettings;
    const bool &m_cancel;
    tstring m_plonkPath;
    tstring m_polipoPath;
    PROCESS_INFORMATION m_plonkProcessInfo;
    PROCESS_INFORMATION m_polipoProcessInfo;
    HANDLE m_polipoPipe;
    int m_connectType;
    map<string, int> m_pageViewEntries;
};
