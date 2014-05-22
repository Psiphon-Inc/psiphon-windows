/*
 * Copyright (c) 2013, Psiphon Inc.
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
#include "meek.h"

class SessionInfo;
class PlonkConnection;


//
// Base class for the SSH transports
//
class SSHTransportBase: public ITransport
{
public:
    SSHTransportBase(LPCTSTR transportProtocolName); 
    virtual ~SSHTransportBase();

    // Subclasses must implement these members
    virtual tstring GetTransportProtocolName() const = 0;
    virtual tstring GetTransportDisplayName() const = 0;
    virtual bool IsHandshakeRequired() const;
    virtual bool IsServerRequestTunnelled() const;
    virtual bool IsSplitTunnelSupported() const;
    virtual bool ServerHasCapabilities(const ServerEntry& entry) const = 0;

    virtual tstring GetSessionID(const SessionInfo& sessionInfo);
    virtual int GetLocalProxyParentPort() const;
    virtual tstring GetLastTransportError() const;
    virtual bool GetUserParentProxySettings(
        bool firstServer,
        const SessionInfo& sessionInfo,
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

    virtual void GetSSHParams(
        int meekListenPort,
        bool firstServer,
        const SessionInfo& sessionInfo,
        const int localSocksProxyPort,
        SystemProxySettings* systemProxySettings,
        tstring& o_serverAddress, 
        int& o_serverPort, 
        tstring& o_serverHostKey, 
        tstring& o_transportRequestName,
        tstring& o_plonkCommandLine) = 0;

    virtual bool IsHandshakeRequired(const ServerEntry& entry) const = 0;

    void TransportConnectHelper();
    bool InitiateConnection(
        int meekListenPort,
        bool firstServer,
        const SessionInfo& sessionInfo,
        boost::shared_ptr<PlonkConnection>& o_plonkConnection);

protected:
    tstring m_plonkPath;
    int m_localSocksProxyPort;

    boost::shared_ptr<PlonkConnection> m_currentPlonk;
    boost::shared_ptr<PlonkConnection> m_previousPlonk;

    Meek* m_meekClient;
};


//
// Standard SSH
//
class SSHTransport: public SSHTransportBase
{
public:
    SSHTransport(); 
    virtual ~SSHTransport();

    static void GetFactory(
                    tstring& o_transportDisplayName,
                    tstring& o_transportProtocolName,
                    TransportFactoryFn& o_transportFactory, 
                    AddServerEntriesFn& o_addServerEntriesFn);

    virtual tstring GetTransportProtocolName() const;
    virtual tstring GetTransportDisplayName() const;
    virtual tstring GetTransportRequestName() const;
    virtual bool ServerHasCapabilities(const ServerEntry& entry) const;

protected:
    virtual void GetSSHParams(
        int meekListenPort,
        bool firstServer,
        const SessionInfo& sessionInfo,
        const int localSocksProxyPort,
        SystemProxySettings* systemProxySettings,
        tstring& o_serverAddress, 
        int& o_serverPort, 
        tstring& o_serverHostKey, 
        tstring& o_transportRequestName,
        tstring& o_plonkCommandLine);
    virtual bool IsHandshakeRequired(const ServerEntry& entry) const;
};


//
// Obfuscated SSH
//
class OSSHTransport: public SSHTransportBase
{
public:
    OSSHTransport(); 
    virtual ~OSSHTransport();

    static void GetFactory(
                    tstring& o_transportDisplayName,
                    tstring& o_transportProtocolName,
                    TransportFactoryFn& o_transportFactory, 
                    AddServerEntriesFn& o_addServerEntriesFn);

    virtual tstring GetTransportProtocolName() const;
    virtual tstring GetTransportDisplayName() const;
    virtual tstring GetTransportRequestName() const;
    virtual bool ServerHasCapabilities(const ServerEntry& entry) const;

protected:
    virtual void GetSSHParams(
        int meekListenPort,
        bool firstServer,
        const SessionInfo& sessionInfo,
        const int localSocksProxyPort,
        SystemProxySettings* systemProxySettings,
        tstring& o_serverAddress, 
        int& o_serverPort, 
        tstring& o_serverHostKey, 
        tstring& o_transportRequestName,
        tstring& o_plonkCommandLine);
    virtual bool IsHandshakeRequired(const ServerEntry& entry) const;

private:
    LPCTSTR m_transportRequestName;
};


