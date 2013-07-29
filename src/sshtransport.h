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

#include "transport.h"
#include "transport_registry.h"
#include "usersettings.h"

class SessionInfo;
class PlonkConnection;


//
// Base class for the SSH transports
//
class SSHTransportBase: public ITransport
{
public:
    SSHTransportBase(); 
    virtual ~SSHTransportBase();

    // Subclasses must implement these members
    virtual tstring GetTransportProtocolName() const = 0;
    virtual tstring GetTransportDisplayName() const = 0;
    virtual bool IsHandshakeRequired(const ServerEntry& entry) const = 0;
    virtual bool IsServerRequestTunnelled() const;
    virtual bool IsSplitTunnelSupported() const;
    virtual unsigned int GetMultiConnectCount() const;
    virtual bool ServerHasCapabilities(const ServerEntry& entry) const;

    virtual tstring GetSessionID(const SessionInfo& sessionInfo);
    virtual int GetLocalProxyParentPort() const;
    virtual tstring GetLastTransportError() const;
    virtual bool GetUserParentProxySettings(
        SystemProxySettings* systemProxySettings,
        tstring& o_UserParentProxyType,
        tstring& o_UserParentProxyHostname,
        int& o_UserParentProxyPort,
        tstring& o_UserParentProxyUsername,
        tstring& o_UserParentProxyPassword);

    virtual void ProxySetupComplete();

    virtual bool Cleanup();

protected:
    // ITransport implementation
    virtual void TransportConnect();
    virtual bool DoPeriodicCheck();

    virtual void GetSSHParamsa(
        const SessionInfo& sessionInfo,
        const int localSocksProxyPort,
        SystemProxySettings* systemProxySettings,
        tstring& o_serverAddress, 
        int& o_serverPort, 
        tstring& o_serverHostKey, 
        tstring& o_plonkCommandLine);
    virtual int GetPort(const SessionInfo& sessionInfo) const = 0;

    void TransportConnectHelper();
    bool GetConnectionServerEntries(ServerEntries& o_serverEntries);
    bool InitiateConnection(
        const SessionInfo& sessionInfo,
        auto_ptr<PlonkConnection>& o_plonkConnection);

protected:
    tstring m_plonkPath;
    int m_localSocksProxyPort;

    auto_ptr<PlonkConnection> m_currentPlonk;
    auto_ptr<PlonkConnection> m_previousPlonk;
};


//
// Standard SSH
//
class SSHTransport: public SSHTransportBase
{
public:
    SSHTransport(); 
    virtual ~SSHTransport();

    static void GetFactory(tstring& o_transportName, TransportFactory& o_transportFactory);

    virtual tstring GetTransportProtocolName() const;
    virtual tstring GetTransportDisplayName() const;

    virtual bool IsHandshakeRequired(const ServerEntry& entry) const;

protected:
    virtual void GetSSHParams(
        const SessionInfo& sessionInfo,
        const int localSocksProxyPort,
        SystemProxySettings* systemProxySettings,
        tstring& o_serverAddress, 
        int& o_serverPort, 
        tstring& o_serverHostKey, 
        tstring& o_plonkCommandLine);
    virtual int GetPort(const SessionInfo& sessionInfo) const;
};


//
// Obfuscated SSH
//
class OSSHTransport: public SSHTransportBase
{
public:
    OSSHTransport(); 
    virtual ~OSSHTransport();

    static void GetFactory(tstring& o_transportName, TransportFactory& o_transportFactory);

    virtual tstring GetTransportProtocolName() const;
    virtual tstring GetTransportDisplayName() const;

    virtual bool IsHandshakeRequired(const ServerEntry& entry) const;

protected:
    virtual void GetSSHParams(
        const SessionInfo& sessionInfo,
        const int localSocksProxyPort,
        SystemProxySettings* systemProxySettings,
        tstring& o_serverAddress, 
        int& o_serverPort, 
        tstring& o_serverHostKey, 
        tstring& o_plonkCommandLine);
    virtual int GetPort(const SessionInfo& sessionInfo) const;
};
