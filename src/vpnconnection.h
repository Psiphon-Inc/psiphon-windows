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

#include "ras.h"
#include "tstring.h"


enum VPNConnectionState
{
    VPN_CONNECTION_STATE_STOPPED = 0,
    VPN_CONNECTION_STATE_STARTING,
    VPN_CONNECTION_STATE_CONNECTED,
    VPN_CONNECTION_STATE_FAILED
};


class VPNConnection
{
public:
    VPNConnection(void);
    virtual ~VPNConnection(void);

    void SetState(VPNConnectionState newState);
    VPNConnectionState GetState(void);
    HANDLE GetStateChangeEvent(void) {return m_stateChangeEvent;}
    tstring GetPPPIPAddress(void);

    bool Establish(const tstring& serverAddress, const tstring& PSK);
    bool Remove(void);
    void SuspendTeardownForUpgrade(void) {m_suspendTeardownForUpgrade = true;}

    void SetLastVPNErrorCode(unsigned int lastErrorCode) {m_lastVPNErrorCode = lastErrorCode;}
    unsigned int GetLastVPNErrorCode(void) {return m_lastVPNErrorCode;}

private:
    HANDLE m_stateChangeEvent;
    VPNConnectionState m_state;
    HRASCONN m_rasConnection;
    bool m_suspendTeardownForUpgrade;
    unsigned int m_lastVPNErrorCode;

    HRASCONN GetActiveRasConnection(void);
};

void TweakVPN(void);
void TweakDNS(void);

