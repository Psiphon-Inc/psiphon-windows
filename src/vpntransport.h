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

#include "ras.h"
#include "transport.h"
#include "transport_registry.h"

class SessionInfo;


class VPNTransport: public ITransport
{
    enum ConnectionState
    {
        CONNECTION_STATE_STOPPED = 0,
        CONNECTION_STATE_STARTING,
        CONNECTION_STATE_CONNECTED,
        CONNECTION_STATE_FAILED
    };

public:
    VPNTransport(); 
    virtual ~VPNTransport();

    static void GetFactory(tstring& o_transportName, TransportFactory& o_transportFactory);

    virtual tstring GetTransportName() const;
    virtual tstring GetSessionID(SessionInfo sessionInfo) const;
    virtual int GetLocalProxyParentPort() const;
    virtual tstring GetLastTransportError() const;

    virtual bool Cleanup();

protected:
    // ITransport implementation
    virtual void TransportConnect(const SessionInfo& sessionInfo);
    virtual bool DoPeriodicCheck();
    
    void TransportConnectHelper(const SessionInfo& sessionInfo);
    bool ServerVPNCapable(const SessionInfo& sessionInfo) const;
    ConnectionState GetConnectionState() const;
    void SetConnectionState(ConnectionState newState);
    HANDLE GetStateChangeEvent();
    void SetLastErrorCode(unsigned int lastErrorCode);
    unsigned int GetLastErrorCode() const;
    bool WaitForConnectionStateToChangeFrom(ConnectionState state, DWORD timeout);
    tstring GetPPPIPAddress() const;
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
    unsigned int m_lastErrorCode;
};
