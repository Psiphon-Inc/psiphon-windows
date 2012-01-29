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

#include "connectionmanager.h"


static int VPN_CONNECTION_TIMEOUT_SECONDS = 20;
static const TCHAR* VPN_CONNECTION_NAME = _T("Psiphon3");


class VPNTransport: public TransportBase
{
    enum ConnectionState
    {
        CONNECTION_STATE_STOPPED = 0,
        CONNECTION_STATE_STARTING,
        CONNECTION_STATE_CONNECTED,
        CONNECTION_STATE_FAILED
    };

public:
    VPNTransport(ConnectionManager* manager); 
    virtual ~VPNTransport();

    virtual tstring GetTransportName() const { return _T("VPN"); }

    virtual void WaitForDisconnect();
    virtual bool Cleanup(bool restartImminent);

protected:
    virtual bool TransportConnect(const ServerEntry& serverEntry);
    
    ConnectionState GetConnectionState() const;
    void SetConnectionState(ConnectionState newState);
    HANDLE GetStateChangeEvent();
    void SetLastVPNErrorCode(unsigned int lastErrorCode);
    unsigned int GetLastVPNErrorCode();
    void WaitForConnectionStateToChangeFrom(ConnectionState state);
    tstring GetPPPIPAddress();
    HRASCONN GetActiveRasConnection();
    bool Establish(const tstring& serverAddress, const tstring& PSK);
    static void CALLBACK RasDialCallback(
                            DWORD userData,
                            DWORD,
                            HRASCONN rasConnection,
                            UINT,
                            RASCONNSTATE rasConnState,
                            DWORD dwError,
                            DWORD);


private:
    ConnectionState m_state;
    HANDLE m_stateChangeEvent;
    HRASCONN m_rasConnection;
    unsigned int m_lastVPNErrorCode;
};
