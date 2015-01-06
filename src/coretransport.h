/*
 * Copyright (c) 2015, Psiphon Inc.
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
#include "transport.h"
#include "transport_registry.h"
#include "usersettings.h"

class SessionInfo;


//
// Base class for the tunnel core transports
//
class CoreTransportBase : public ITransport
{
public:
    CoreTransportBase(LPCTSTR transportProtocolName); 
    virtual ~CoreTransportBase();

    virtual bool Cleanup();
    virtual tstring GetTransportProtocolName() const = 0;
    virtual tstring GetTransportDisplayName() const = 0;
    virtual bool IsHandshakeRequired() const;
    virtual bool IsWholeSystemTunneled() const;
    virtual bool IsSplitTunnelSupported() const;
    virtual bool ServerWithCapabilitiesExists();
    virtual bool ServerHasCapabilities(const ServerEntry& entry) const;
    virtual bool RequiresStatsSupport() const;
    virtual tstring GetSessionID(const SessionInfo& sessionInfo);
    virtual int GetLocalProxyParentPort() const;
    virtual tstring GetLastTransportError() const;

protected:
    virtual void TransportConnect();
    virtual bool DoPeriodicCheck();

    void TransportConnectHelper();
	bool WriteConfigFile(tstring& configFilename);
    string GetUpstreamProxyAddress();
    bool SpawnCoreProcess(const tstring& configFilename);
    bool CreateCoreProcessPipe(HANDLE& o_outputPipe, HANDLE& o_errorPipe);
    void ConsumeCoreProcessOutput();
    void HandleCoreProcessOutputLine(const char* line);

protected:
    tstring m_exePath;
    int m_localSocksProxyPort;
    int m_localHttpProxyPort;
    // [homepages]
    // [upgrade version#]
    PROCESS_INFORMATION m_processInfo;
    HANDLE m_pipe;
	string m_pipeBuffer;
	bool m_tunnelActive;
};


//
// Standard SSH
//
class SSHTransport : public CoreTransportBase
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

protected:
};


//
// Obfuscated SSH
//
class OSSHTransport : public CoreTransportBase
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

protected:
};


