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

#include "stdafx.h"
#include "transport_connection.h"
#include "systemproxysettings.h"
#include "local_proxy.h"
#include "transport.h"
#include "psiclient.h"


TransportConnection::TransportConnection()
    : m_transport(0),
      m_localProxy(0),
      m_skipApplySystemProxySettings(false)
{
}

TransportConnection::~TransportConnection()
{
    try
    {
        Cleanup();
    }
    catch (...)
    {
    }
}

SessionInfo TransportConnection::GetUpdatedSessionInfo() const
{
    return m_sessionInfo;
}

void TransportConnection::Connect(
                            const StopInfo& stopInfo,
                            ITransport* transport,
                            IReconnectStateReceiver* reconnectStateReceiver,
                            ILocalProxyStatsCollector* statsCollector, 
                            ServerEntry* tempConnectServerEntry/*=NULL*/,
                            bool skipApplySystemProxySettings/* = false*/)
{
    assert(m_transport == 0);
    assert(m_localProxy == 0);

    assert(transport);
    assert(!tempConnectServerEntry || !transport->IsHandshakeRequired());

    m_skipApplySystemProxySettings = skipApplySystemProxySettings;

    m_transport = transport;

    try
    {
        m_workerThreadSynch.Reset();

        // Connect with the transport. Will throw on error.
        m_transport->Connect(
                    &m_systemProxySettings,
                    stopInfo,
                    reconnectStateReceiver,
                    &m_workerThreadSynch,
                    tempConnectServerEntry);

        // Get initial SessionInfo. Note that this might be pre-handshake
        // and therefore not be totally filled in.
        m_sessionInfo = m_transport->GetSessionInfo();

        if (m_transport->RequiresStatsSupport())
        {
            // Set up and start the local proxy.
            m_localProxy = new LocalProxy(
                                statsCollector, 
                                m_sessionInfo.GetServerAddress().c_str(), 
                                &m_systemProxySettings,
                                0, // no parent port
                                tstring()); // no split tunnel file path

            // Launches the local proxy thread and doesn't return until it
            // observes a successful (or not) connection.
            if (!m_localProxy->Start(stopInfo, &m_workerThreadSynch))
            {
                throw IWorkerThread::Error("LocalProxy::Start failed");
            }
        }

        // In the case of the URL proxy, which might be created while another transport
        // is running, we don't want to change the system proxy settings or change the registry
        // keys that hold any information about proxy settings.
        if (!m_skipApplySystemProxySettings)
        {
            // If the whole system is tunneled (i.e., VPN), then we can't leave the
            // original system proxy settings intact -- because that proxy would 
            // (probably) not be reachable in VPN mode and the user would 
            // effectively have no connectivity.
            bool allowedToSkipProxySettings = !m_transport->IsWholeSystemTunneled();

            // Apply the system proxy settings that have been collected by the transport
            // and the local proxy.
            if (!m_systemProxySettings.Apply(allowedToSkipProxySettings))
            {
                throw IWorkerThread::Error("SystemProxySettings::Apply failed");
            }
        }

        // If the transport did a handshake, there may be updated session info.
        m_sessionInfo = m_transport->GetSessionInfo();

        // Update the session info with local proxy ports.
        m_sessionInfo.SetLocalProxyPorts(
            m_systemProxySettings.GetHttpProxyPort(), 
            m_systemProxySettings.GetHttpsProxyPort(), 
            m_systemProxySettings.GetSocksProxyPort());

        // Now that we have extra info from the server via the handshake 
        // (specifically page view regexes), we need to update the local proxy.
        if (m_localProxy)
        {
            m_localProxy->UpdateSessionInfo(m_sessionInfo);
        }
    }
    catch (ITransport::TransportFailed&)
    {
        Cleanup();

        // Check for stop signal and throw if it's set
        stopInfo.stopSignal->CheckSignal(stopInfo.stopReasons, true);

        if (m_transport->IsConnectRetryOkay())
        {
            throw TransportConnection::TryNextServer();
        }
        throw TransportConnection::PermanentFailure();
    }
    catch(...)
    {
        // Make sure the transport and proxy are cleaned up and then just rethrow
        Cleanup();

        throw;
    }
}

void TransportConnection::WaitForDisconnect()
{
    HANDLE waitHandles[2];
    size_t waitHandlesCount = 1;
    waitHandles[0] = m_transport->GetStoppedEvent();
    if (m_localProxy)
    {
        waitHandles[1] = m_localProxy->GetStoppedEvent();
        waitHandlesCount += 1;
    }

    DWORD result = WaitForMultipleObjects(
                    waitHandlesCount, 
                    waitHandles, 
                    FALSE, // wait for any event
                    INFINITE);

    Cleanup();

    if (result > (WAIT_OBJECT_0 + waitHandlesCount - 1))
    {
        throw IWorkerThread::Error("WaitForMultipleObjects failed");
    }
}


void TransportConnection::Cleanup()
{
    if (!m_skipApplySystemProxySettings)
    {
        // NOTE: It is important that the system proxy settings get torn down
        // before the transport and local proxy do. Otherwise, all web connections
        // will have a window of being guaranteed to fail (including and especially
        // our own -- like final /status requests).
        m_systemProxySettings.Revert();
    }

    if (m_localProxy) 
    {
        m_localProxy->Stop();
        delete m_localProxy;
    }
    m_localProxy = 0;

    if (m_transport)
    {
        m_transport->Stop();
        m_transport->Cleanup();
    }
}
