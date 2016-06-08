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

/*
NOTES
- Make behaviour consistent when user cancels connect. 
  Right now it might throw abort or might exit cleanly (SSH for sure).
*/

#pragma once

#include "worker_thread.h"
#include "sessioninfo.h"


class SystemProxySettings;


class IReconnectStateReceiver
{
public:
    virtual void SetReconnecting() = 0;
    virtual void SetReconnected() = 0;
};


// All transport implementations must implement this interface
class ITransport : public IWorkerThread
{
public:
    ITransport(LPCTSTR transportProtocolName);

    //returns immutable transport protocol name 
    virtual tstring GetTransportProtocolName() const = 0;

    //returns immutable transport display name 
    virtual tstring GetTransportDisplayName() const = 0;

    //transport request name can be changed by the class methods, 
    //this is the one used in psiphon web requests
    virtual tstring GetTransportRequestName() const = 0;

    // TransportRegistry functions. 
    // Every implementing class must have a static function with this signature:
    //static void GetFactory(
    //              tstring& o_transportDisplayName,
    //              tstring& o_transportProtocolName,
    //              TransportFactoryFn& o_transportFactoryFn, 
    //              AddServerEntriesFn& o_addServerEntriesFn);

    // Only valid when connected
    virtual tstring GetSessionID(const SessionInfo& sessionInfo) = 0;

    // Returns true if:
    // - needs a local HTTP proxy to be run (used to proxy Psiphon API web requests, etc.)
    // - needs assistance in calling /connected and /status requests
    virtual bool RequiresStatsSupport() const = 0;

    virtual tstring GetLastTransportError() const = 0;

    // Returns true if pre-handshake is required to connect, false otherwise.
    virtual bool IsHandshakeRequired() const = 0;

    // Returns true if all system traffic is tunneled, rather than only traffic
    // routed through the local Psiphon proxy. 
    // In other words, this is true for VPN, false for SSH.
    virtual bool IsWholeSystemTunneled() const = 0;

    // Returns true if at least one server supports this transport.
    virtual bool ServerWithCapabilitiesExists();

    // Returns true if the specified server supports this transport.
    virtual bool ServerHasCapabilities(const ServerEntry& entry) const = 0;

    // Call to create the connection.
    // A failed attempt must clean itself up as needed.
    // May throw TransportFailed or Abort.
    // Subclasses must not override.
    // remoteServerListFetcher is optional. If NULL, no remote server list 
    //   fetch will be triggered.
    void Connect(
            SystemProxySettings* systemProxySettings,
            const StopInfo& stopInfo,
            IReconnectStateReceiver* reconnectStateReceiver,
            WorkerThreadSynch* workerThreadSynch,
            ServerEntry* tempConnectServerEntry=NULL);

    // Returns true if it's okay to retry the connection using the same transport
    // and connection parameters. If it returns false, then the failure is permanent.
    bool IsConnectRetryOkay() const;

    // Do any necessary final cleanup. 
    // Must be safe to call even if a connection was never established.
    virtual bool Cleanup() = 0;

    // If `andNotTemporary` is true, the transport will only be considered
    // connected if it's not just a temporary connection.
    bool IsConnected(bool andNotTemporary) const;

    // Must be called after connecting, if there has been a handshake that 
    // added more data to sessionInfo.
    SessionInfo GetSessionInfo() const;

    static size_t AddServerEntries(
            LPCTSTR transportProtocolName,
            const vector<string>& newServerEntryList, 
            const ServerEntry* serverEntry);

    //
    // Exception classes
    //
    // Generally speaking, any of these, or IWorkerThread::Abort, or
    // IWorkerThread::Error may be thrown at any time. 
    // (Except in const members?)
    //
    // Indicates that this transport was not successful.
    // If connectRetryOkay is not true, then there should not be another connection
    // attempt made with the transport using the same parameters.
    class TransportFailed 
    { 
        friend class ITransport;
    public:
        TransportFailed() : m_connectRetryOkay(true) {}
        TransportFailed(bool connectRetryOkay) : m_connectRetryOkay(connectRetryOkay) {}
    protected:
        bool m_connectRetryOkay;
    };

protected:
    // May throw TransportFailed or IWorkerThread::Abort
    virtual void TransportConnect() = 0;

    // IWorkerThread implementation
    virtual bool DoStart();
    virtual void StopImminent();
    virtual void DoStop(bool cleanly);
    // The implementing class must implement this
    virtual bool DoPeriodicCheck() = 0;

    void MarkServerSucceeded(const ServerEntry& serverEntry);
    void MarkServerFailed(const ServerEntry& serverEntry);

    tstring GetHandshakeRequestPath(const SessionInfo& sessionInfo);
    // May throw StopSignal::StopException
    bool DoHandshake(bool preTransport, SessionInfo& sessionInfo);

protected:
    SessionInfo m_sessionInfo;
    SystemProxySettings* m_systemProxySettings;
    ServerEntry* m_tempConnectServerEntry;
    ServerList m_serverList;
    bool m_firstConnectionAttempt;
    IReconnectStateReceiver* m_reconnectStateReceiver;
    bool m_connectRetryOkay;
};
