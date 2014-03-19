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
#include "meek.h"


TransportConnection::TransportConnection()
    : m_transport(0),
      m_localProxy(0),
	  m_meekClient(0)

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
                            ILocalProxyStatsCollector* statsCollector, 
                            IRemoteServerListFetcher* remoteServerListFetcher,
                            const tstring& splitTunnelingFilePath,
                            ServerEntry* tempConnectServerEntry/*=NULL*/)
{
    assert(m_transport == 0);
    assert(m_localProxy == 0); 
	assert(m_meekClient == 0);


    assert(transport);
    assert(!tempConnectServerEntry || !transport->IsHandshakeRequired());

    m_transport = transport;

    try
    {
        // Delete any leftover split tunnelling rules
        if(splitTunnelingFilePath.length() > 0 && !DeleteFile(splitTunnelingFilePath.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
        {
            throw std::exception("TransportConnection::Connect - DeleteFile failed");
        }

        m_workerThreadSynch.Reset();

		//Start meek client and get its listening port

		m_meekClient = new Meek(_T("http://meek.psiphon.ca"), _T("www.cloudflare.com"));

		if (!m_meekClient->Start(stopInfo, &m_workerThreadSynch))
        {
            throw IWorkerThread::Error("Meek::Start failed");
        }

		if(!m_meekClient->WaitForCmethodLine())
		{
			throw IWorkerThread::Error("Meek::CMETHOD not available");
		}

        // Connect with the transport. Will throw on error.
		m_transport->Connect(
					m_meekClient->GetListenPort(),
                    &m_systemProxySettings,
                    stopInfo,
                    &m_workerThreadSynch,
                    remoteServerListFetcher,
                    tempConnectServerEntry);

        // Get initial SessionInfo. Note that this might be pre-handshake
        // and therefore not be totally filled in.
        m_sessionInfo = m_transport->GetSessionInfo();

        // Set up and start the local proxy.
        m_localProxy = new LocalProxy(
                            statsCollector, 
                            m_sessionInfo.GetServerAddress().c_str(), 
                            &m_systemProxySettings,
                            m_transport->GetLocalProxyParentPort(), 
                            splitTunnelingFilePath);

        // Launches the local proxy thread and doesn't return until it
        // observes a successful (or not) connection.
        if (!m_localProxy->Start(stopInfo, &m_workerThreadSynch))
        {
            throw IWorkerThread::Error("LocalProxy::Start failed");
        }

        // Apply the system proxy settings that have been collected by the transport
        // and the local proxy.
        if (!m_systemProxySettings.Apply())
        {
            throw IWorkerThread::Error("SystemProxySettings::Apply failed");
        }

        // Let the transport do a handshake
        m_transport->ProxySetupComplete();

        // If the transport did a handshake, there may be updated session info.
        m_sessionInfo = m_transport->GetSessionInfo();

        // Now that we have extra info from the server via the handshake 
        // (specifically page view regexes), we need to update the local proxy.
        m_localProxy->UpdateSessionInfo(m_sessionInfo);
    }
    catch (ITransport::TransportFailed&)
    {
        Cleanup();

        // Check for stop signal and throw if it's set
        stopInfo.stopSignal->CheckSignal(stopInfo.stopReasons, true);

        // We don't fail over transports, so...
        throw TransportConnection::TryNextServer();
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
    HANDLE waitHandles[] = { m_transport->GetStoppedEvent(), 
                             m_localProxy->GetStoppedEvent() };
    size_t waitHandlesCount = sizeof(waitHandles)/sizeof(HANDLE);

    DWORD result = WaitForMultipleObjects(
                    waitHandlesCount, 
                    waitHandles, 
                    FALSE, // wait for any event
                    INFINITE);

    // One of the transport or the local proxy has stopped. 
    // Make sure they both are.
    m_localProxy->Stop();
    m_transport->Stop();

    Cleanup();

    if (result > (WAIT_OBJECT_0 + waitHandlesCount - 1))
    {
        throw IWorkerThread::Error("WaitForMultipleObjects failed");
    }
}


void TransportConnection::Cleanup()
{
    // NOTE: It is important that the system proxy settings get torn down
    // before the transport and local proxy do. Otherwise, all web connections
    // will have a window of being guaranteed to fail (including and especially
    // our own -- like final /status requests).
    m_systemProxySettings.Revert();

    if (m_transport)
    {
        m_transport->Stop();
        m_transport->Cleanup();
    }

    if (m_localProxy) delete m_localProxy;
    m_localProxy = 0;

	if(m_meekClient) delete m_meekClient;
	m_meekClient = 0;
}
