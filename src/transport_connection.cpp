/*
 * Copyright (c) 2011, Psiphon Inc.
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
#include "server_request.h"
#include "local_proxy.h"
#include "transport.h"
#include "psiclient.h"



TransportConnection::TransportConnection()
    : m_transport(0),
      m_localProxy(0)
{
}

TransportConnection::~TransportConnection()
{
    Cleanup();
}

void TransportConnection::Connect(
                            ITransport* transport,
                            ILocalProxyStatsCollector* statsCollector, 
                            const SessionInfo& sessionInfo, 
                            const TCHAR* handshakeRequestPath,
                            const tstring& splitTunnelingFilePath, 
                            const bool& stopSignalFlag)
{
    assert(m_transport == 0);
    assert(m_localProxy == 0); 

    m_sessionInfo = sessionInfo;
    bool handshakeDone = false;

    try
    {
        // Some transports require a handshake before connecting; with others we
        // can connect before doing the handshake.    
        if (m_transport->IsHandshakeRequired(m_sessionInfo))
        {
            if (!handshakeRequestPath)
            {
                // Need a handshake but can't do a handshake.
                throw TryNextServer();
            }

            DoHandshake(handshakeRequestPath, stopSignalFlag);
            handshakeDone = true;
        }

        m_referenceCounter.Reset();

        // Connect with the transport.
        // May throw.
        m_transport->Connect(
                    m_sessionInfo, 
                    &m_systemProxySettings,
                    stopSignalFlag,
                    &m_referenceCounter);

        // Set up and start the local proxy.
        m_localProxy = new LocalProxy(
                            statsCollector, 
                            m_sessionInfo, 
                            &m_systemProxySettings,
                            m_transport->GetLocalProxyParentPort(), 
                            splitTunnelingFilePath);

        // Launches the local proxy thread and doesn't return until it
        // observes a successful (or not) connection.
        if (!m_localProxy->Start(stopSignalFlag, &m_referenceCounter))
        {
            throw IWorkerThread::Error("LocalProxy::Start failed");
        }

        // Apply the system proxy settings that have been collected by the transport
        // and the local proxy.
        m_systemProxySettings.Apply();

        // If we didn't do the handshake before, do it now.
        if (!handshakeDone)
        {
            DoHandshake(handshakeRequestPath, stopSignalFlag);
            handshakeDone = true;
        }

        // Now that we have extra info from the server via the handshake 
        // (specifically page view regexes), we need to update the local proxy.
        m_localProxy->UpdateSessionInfo(m_sessionInfo);
    }
    catch (ITransport::TransportFailed&)
    {
        Cleanup();

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

void TransportConnection::DoHandshake(
                            const TCHAR* handshakeRequestPath,
                            const bool& stopSignalFlag)
{
    ServerRequest serverRequest;
    string handshakeResponse;

    // Send list of known server IP addresses (used for stats logging on the server)

    (void)serverRequest.MakeRequest(
                        stopSignalFlag,
                        m_transport,
                        m_sessionInfo,
                        handshakeRequestPath,
                        handshakeResponse);

    if (handshakeResponse.length() > 0)
    {
        if (!m_sessionInfo.ParseHandshakeResponse(handshakeResponse.c_str()))
        {
            my_print(false, _T("%s: ParseHandshakeResponse failed"), __TFUNCTION__);
            throw TryNextServer();
        }
    }
    else
    {
        my_print(false, _T("%s: handshake failed"), __TFUNCTION__);
        throw TryNextServer();
    }
}

void TransportConnection::Cleanup()
{
    if (m_transport) m_transport->Cleanup();
    if (m_localProxy) delete m_localProxy;
    m_localProxy = 0;
}
