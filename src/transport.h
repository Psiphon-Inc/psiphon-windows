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


class ITransport;
class SystemProxySettings;


// This should be implemented by something like ConnectionManager to provide a 
// way for Transports to (optionally) trigger remote server list fetches 
// during connection sequences.
// In concrete terms, SSHTransport can take a long time to try connecting to
// all available servers, so we want to be able to trigger a remote server list
// fetch after a certain amount of time.
class IRemoteServerListFetcher
{
public:
    virtual void FetchRemoteServerList() = 0;
};


// All transport implementations must implement this interface
class ITransport : public IWorkerThread
{
public:
    ITransport(LPCTSTR transportProtocolName);

    virtual tstring GetTransportProtocolName() const = 0;

    virtual tstring GetTransportDisplayName() const = 0;

    // TransportRegistry functions. 
    // Every implementing class must have a static function with this signature:
    //static void GetFactory(
    //              tstring& o_transportDisplayName,
    //              tstring& o_transportProtocolName,
    //              TransportFactoryFn& o_transportFactoryFn, 
    //              AddServerEntriesFn& o_addServerEntriesFn);

    // Only valid when connected
    virtual tstring GetSessionID(const SessionInfo& sessionInfo) = 0;

    // Find out what port, if any, the local proxy should connect to in order 
    // to use this transport.
    // Returns zero if the local proxy should not connect directly to the transport.
    virtual int GetLocalProxyParentPort() const = 0;

    virtual tstring GetLastTransportError() const = 0;

    // Returns true if pre-handshake is required to connect, false otherwise.
    virtual bool IsHandshakeRequired() const = 0;

    // Returns true if requests to the server should be tunnelled/proxied
    // through the transport. If not, then the local proxy should not be used.
    virtual bool IsServerRequestTunnelled() const = 0;

    // Returns true if split tunnelling is supported for the transport.
    virtual bool IsSplitTunnelSupported() const = 0;

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
            WorkerThreadSynch* workerThreadSynch,
            IRemoteServerListFetcher* remoteServerListFetcher,
            ServerEntry* tempConnectServerEntry=NULL);

    // Do any necessary final cleanup. 
    // Must be safe to call even if a connection was never established.
    virtual bool Cleanup() = 0;

    bool IsConnected() const;

    // Must be called after connecting, if there has been a handshake that 
    // added more data to sessionInfo.
    SessionInfo GetSessionInfo() const;

    // Must be called after the local proxy is running and the system proxy
    // settings are in place. 
    // Subclasses may use this opportunity to make a handshake.
    virtual void ProxySetupComplete() = 0;

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
    // Indicates that this transport was not successful
    class TransportFailed { };

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
    bool DoHandshake(bool preTransport, SessionInfo& sessionInfo);

protected:
    SessionInfo m_sessionInfo;
    SystemProxySettings* m_systemProxySettings;
    ServerEntry* m_tempConnectServerEntry;
    ServerList m_serverList;
    IRemoteServerListFetcher* m_remoteServerListFetcher;
    bool m_firstConnectionAttempt;
};
