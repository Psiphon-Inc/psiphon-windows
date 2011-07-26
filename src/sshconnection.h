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

class SSHConnection
{
public:
    SSHConnection(const bool& cancel);
    virtual ~SSHConnection(void);

    // TODO: async connect, state change notifiers, connection monitor, etc.?

    bool Connect(
        const tstring& sshServerAddress,
        const tstring& sshServerPort,
        const tstring& sshServerPublicKey,
        const tstring& sshUsername,
        const tstring& sshPassword);
    void Disconnect(void);
    bool WaitForConnected(void);
    void WaitAndDisconnect(void);
    void SignalDisconnect(void);

private:
    SystemProxySettings m_systemProxySettings;
    const bool &m_cancel;
    tstring m_plinkPath;
    tstring m_polipoPath;
    PROCESS_INFORMATION m_plinkProcessInfo;
    PROCESS_INFORMATION m_polipoProcessInfo;
};
