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
#include "transport.h"
#include "sessioninfo.h"
#include "psiclient.h"


/******************************************************************************
 ITransport
******************************************************************************/

ITransport::ITransport()
    : m_systemProxySettings(NULL)
{
}

void ITransport::Connect(
                    SessionInfo sessionInfo, 
                    SystemProxySettings* systemProxySettings,
                    const bool& stopSignalFlag)
{
    m_sessionInfo = sessionInfo;
    m_systemProxySettings = systemProxySettings;
    assert(m_systemProxySettings);

    if (!IWorkerThread::Start(stopSignalFlag))
    {
        throw TransportFailed();
    }
}

bool ITransport::DoStart()
{
    try
    {
        TransportConnect(m_sessionInfo, m_systemProxySettings);
    }
    catch(...)
    {
        return false;
    }

    my_print(false, _T("%s successfully connected."), GetTransportDisplayName().c_str());

    return true;
}

void ITransport::DoStop()
{
    Cleanup();
    m_systemProxySettings = 0;

    my_print(false, _T("%s disconnected."), GetTransportDisplayName().c_str());
}

bool ITransport::IsConnected() const
{
    return IsRunning();
}


/******************************************************************************
 TransportConnection
******************************************************************************/

#include "systemproxysettings.h"
#include "server_request.h"
#include "local_proxy.h"

TransportConnection::TransportConnection()
    : m_transport(0),
      m_localProxy(0)
{
}

TransportConnection::~TransportConnection()
{
    if (m_transport) m_transport->Cleanup();
    if (m_localProxy) delete m_localProxy;
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

    // Some transports require a handshake before connecting; with others we
    // can connect before doing the handshake.    
    if (m_transport->IsHandshakeRequired(m_sessionInfo))
    {
        DoHandshake(handshakeRequestPath, stopSignalFlag);
        handshakeDone = true;
    }

    // Connect with the transport.
    m_transport->Connect(
                m_sessionInfo, 
                &m_systemProxySettings,
                stopSignalFlag);

    // Set up and start the local proxy.
    m_localProxy = new LocalProxy(
                        statsCollector, 
                        m_sessionInfo, 
                        &m_systemProxySettings,
                        m_transport->GetLocalProxyParentPort(), 
                        splitTunnelingFilePath);

    // Launches the local proxy thread and doesn't return until it
    // observes a successful (or not) connection.
    if (!m_localProxy->Start(stopSignalFlag))
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
    }

    // Now that we have extra info from the server via the handshake 
    // (specifically page view regexes), we need to update the local proxy.
    m_localProxy->UpdateSessionInfo(m_sessionInfo);
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

    m_localProxy->Stop();
    m_transport->Stop();

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

    if (handshakeResponse.length() == 0)
    {
        if (!m_sessionInfo.ParseHandshakeResponse(handshakeResponse.c_str()))
        {
            my_print(false, _T("%s: ParseHandshakeResponse failed", __TFUNCTION__));
            throw TryNextServer();
        }
    }
    else
    {
        my_print(false, _T("%s: handshake failed"), __TFUNCTION__);
        throw TryNextServer();
    }
}
