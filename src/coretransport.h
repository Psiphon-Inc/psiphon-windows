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

#define CORE_TRANSPORT_PROTOCOL_NAME    _T("CoreTransport")
#define CORE_TRANSPORT_DISPLAY_NAME     _T("Psiphon Tunnel")

class CoreTransport : public ITransport
{
public:
    CoreTransport();
    virtual ~CoreTransport();

    static void GetFactory(
                    tstring& o_transportDisplayName,
                    tstring& o_transportProtocolName,
                    TransportFactoryFn& o_transportFactory,
                    AddServerEntriesFn& o_addServerEntriesFn);

    virtual tstring GetTransportProtocolName() const;
    virtual tstring GetTransportDisplayName() const;
    virtual tstring GetTransportRequestName() const;

    virtual bool Cleanup();
    virtual bool IsHandshakeRequired() const;
    virtual bool IsWholeSystemTunneled() const;
    virtual bool SupportsAuthorizations() const override;
    virtual bool ServerWithCapabilitiesExists();
    virtual bool ServerHasCapabilities(const ServerEntry& entry) const;
    virtual bool RequiresStatsSupport() const;
    virtual tstring GetSessionID(const SessionInfo& sessionInfo);
    virtual int GetLocalProxyParentPort() const;
    virtual tstring GetLastTransportError() const;

protected:
    virtual void TransportConnect();
    virtual bool DoPeriodicCheck();

    bool RequestingUrlProxyWithoutTunnel();
    void TransportConnectHelper();
    bool WriteParameterFiles(tstring& configFilename, tstring& serverListFilename, tstring& oldClientUpgradeFilename, tstring& newClientUpgradeFilename);
    string GetUpstreamProxyAddress();
    bool SpawnCoreProcess(const tstring& configFilename, const tstring& serverListFilename);
    void ConsumeCoreProcessOutput();
    bool ValidateAndPaveUpgrade(const tstring clientUpgradeFilename);
    void HandleCoreProcessOutputLine(const char* line);

protected:
    tstring m_exePath;
    int m_localSocksProxyPort;
    int m_localHttpProxyPort;
    PROCESS_INFORMATION m_processInfo;
    HANDLE m_pipe;
    string m_pipeBuffer;
    bool m_hasEverConnected;
    bool m_isConnected;
    bool m_clientUpgradeDownloadHandled;
    string m_lastUpstreamProxyErrorMessage;
    bool m_panicked;
    std::vector<std::string> m_authorizationIDs;
};
