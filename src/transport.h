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


// All transport implementations must implement this interface
class ITransport : public IWorkerThread
{
public:
    ITransport();

    virtual tstring GetTransportProtocolName() const = 0;

    virtual tstring GetTransportDisplayName() const = 0;

    // Every implementing class must have a static function with this signature:
    //static void GetFactory(tstring& o_transportName, TransportFactory& o_transportFactory);

    // Only valid when connected
    virtual tstring GetSessionID(const SessionInfo& sessionInfo) = 0;

    // Find out what port, if any, the local proxy should connect to in order 
    // to use this transport.
    // Returns zero if the local proxy should not connect directly to the transport.
    virtual int GetLocalProxyParentPort() const = 0;

    virtual tstring GetLastTransportError() const = 0;

    // Examines the available information in SessionInfo and determines if a
    // request to the server for further info is needed before this transport
    // can connect.
    virtual bool IsHandshakeRequired(const ServerEntry& entry) const = 0;

    // Returns true if requests to the server should be tunnelled/proxied
    // through the transport. If not, then the local proxy should not be used.
    virtual bool IsServerRequestTunnelled() const = 0;

    // Returns true if split tunnelling is supported for the transport.
    virtual bool IsSplitTunnelSupported() const = 0;

    // Returns the number of servers that this transport is capable of attempting
    // to "multi-connect" to (that is, try to connect to many servers at once, 
    // remaining connected to just one).
    virtual unsigned int GetMultiConnectCount() const = 0;

    // Returns true if at least one server supports this transport.
    virtual bool ServerWithCapabilitiesExists(ServerList& serverList) const;

    // Returns true if the specified server supports this transport.
    virtual bool ServerHasCapabilities(const ServerEntry& entry) const = 0;

    // Call to create the connection.
    // A failed attempt must clean itself up as needed.
    // May throw TransportFailed or Abort
    // Subclasses must not override.
    void Connect(
            const vector<SessionInfo>& sessionInfo, 
            SystemProxySettings* systemProxySettings,
            const StopInfo& stopInfo,
            WorkerThreadSynch* workerThreadSynch,
            int& o_chosenSessionInfoIndex,
            ServerEntries& o_failedServerEntries);

    // Do any necessary final cleanup. 
    // Must be safe to call even if a connection was never established.
    virtual bool Cleanup() = 0;

    bool IsConnected() const;

    // Must be called after connecting, if there has been a handshake that 
    // added more data to sessionInfo.
    void UpdateSessionInfo(const SessionInfo& sessionInfo);

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

    void AddFailedServer(const SessionInfo& sessionInfo);
    void AddFailedServers(const vector<SessionInfo>& sessionInfo);

protected:
    vector<SessionInfo> m_sessionInfo;
    int m_chosenSessionInfoIndex;
    SystemProxySettings* m_systemProxySettings;

private:
    ServerEntries* m_failedServerEntries;
};
