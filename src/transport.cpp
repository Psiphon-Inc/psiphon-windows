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
#include "logging.h"
#include "transport.h"
#include "sessioninfo.h"
#include "psiclient.h"
#include "stopsignal.h"
#include "server_request.h"
#include "embeddedvalues.h"
#include "config.h"
#include "transport_registry.h"
#include "systemproxysettings.h"


/******************************************************************************
 ITransport
******************************************************************************/

ITransport::ITransport(LPCTSTR transportProtocolName)
    : m_systemProxySettings(NULL),
      m_tempConnectServerEntry(NULL),
      m_serverList(WStringToUTF8(transportProtocolName).c_str()),
      m_firstConnectionAttempt(true),
      m_reconnectStateReceiver(NULL),
      m_connectRetryOkay(true)
{
}


bool ITransport::ServerWithCapabilitiesExists()
{
    ServerEntries entries = m_serverList.GetList();

    for (size_t i = 0; i < entries.size(); i++)
    {
        if (ServerHasCapabilities(entries[i]))
        {
            return true;
        }
    }

    return false;
}


void ITransport::Connect(
                    SystemProxySettings* systemProxySettings,
                    const StopInfo& stopInfo,
                    IReconnectStateReceiver* reconnectStateReceiver,
                    WorkerThreadSynch* workerThreadSynch,
                    ServerEntry* tempConnectServerEntry/*=NULL*/)
{
    m_systemProxySettings = systemProxySettings;
    m_tempConnectServerEntry = tempConnectServerEntry;
    m_reconnectStateReceiver = reconnectStateReceiver;
    m_connectRetryOkay = true;

    assert(m_systemProxySettings);

    if (!IWorkerThread::Start(stopInfo, workerThreadSynch))
    {
        throw TransportFailed();
    }
}


bool ITransport::IsConnectRetryOkay() const
{
    return m_connectRetryOkay;
}


SessionInfo ITransport::GetSessionInfo() const
{
    return m_sessionInfo;
}


bool ITransport::DoStart()
{
    try
    {
        TransportConnect();
    }
    catch (TransportFailed& ex)
    {
        m_connectRetryOkay = ex.m_connectRetryOkay;
        m_firstConnectionAttempt = false;
        return false;
    }
    catch(...)
    {
        m_firstConnectionAttempt = false;
        return false;
    }

    m_firstConnectionAttempt = false;
    return true;
}


void ITransport::StopImminent()
{
}


void ITransport::DoStop(bool cleanly)
{
    Cleanup();

    // We'll use the non-null-ness of one of these members as a sign that we
    // were connected.
    if (m_systemProxySettings || m_tempConnectServerEntry)
    {
        my_print(NOT_SENSITIVE, false, _T("%s disconnected."), GetTransportDisplayName().c_str());
    }

    // Set the "unexpected disconnect" signal, so logic higher up can 
    // respond accordingly (e.g., attempt to reconnect).
    // But don't set it if this is a temporary transport, because that will
    // mess up the higher logic. (Bit of a hack.)
    if (!cleanly && !m_tempConnectServerEntry && IsConnected(true)) 
    {
        m_stopInfo.stopSignal->SignalStop(STOP_REASON_UNEXPECTED_DISCONNECT);
    }

    m_systemProxySettings = NULL;
    m_tempConnectServerEntry = NULL;
}


bool ITransport::IsConnected(bool andNotTemporary) const
{
    return IsRunning() && 
           !(andNotTemporary && m_tempConnectServerEntry);
}


void ITransport::MarkServerSucceeded(const ServerEntry& serverEntry)
{
    // Don't mark anything if we're making a temporary connection
    if (m_tempConnectServerEntry)
    {
        return;
    }

    // Force the serverEntry to be at the very front of the server list.
    m_serverList.MoveEntryToFront(serverEntry, true);
}


void ITransport::MarkServerFailed(const ServerEntry& serverEntry)
{
    // Don't mark anything if we're making a temporary connection
    if (m_tempConnectServerEntry)
    {
        return;
    }

    m_serverList.MarkServerFailed(serverEntry);
}


tstring ITransport::GetHandshakeRequestPath(const SessionInfo& sessionInfo)
{
    tstring handshakeRequestPath;
    handshakeRequestPath = tstring(HTTP_HANDSHAKE_REQUEST_PATH) + 
                           _T("?client_session_id=") + UTF8ToWString(sessionInfo.GetClientSessionID()) +
                           _T("&propagation_channel_id=") + UTF8ToWString(PROPAGATION_CHANNEL_ID) +
                           _T("&sponsor_id=") + UTF8ToWString(SPONSOR_ID) +
                           _T("&client_version=") + UTF8ToWString(CLIENT_VERSION) +
                           _T("&server_secret=") + UTF8ToWString(sessionInfo.GetWebServerSecret()) +
                           _T("&relay_protocol=") + GetTransportRequestName();

    // Include a list of known server IP addresses in the request query string as required by /handshake
    ServerEntries serverEntries = m_serverList.GetList();
    for (ServerEntryIterator ii = serverEntries.begin(); ii != serverEntries.end(); ++ii)
    {
        handshakeRequestPath += _T("&known_server=");
        handshakeRequestPath += UTF8ToWString(ii->serverAddress);
    }

    return handshakeRequestPath;
}


bool ITransport::DoHandshake(bool preTransport, SessionInfo& sessionInfo)
{
    string handshakeResponse;

    tstring handshakeRequestPath = GetHandshakeRequestPath(sessionInfo);

    // Send list of known server IP addresses (used for stats logging on the server)

    // Allow an adhoc tunnel if this is a pre-transport handshake (i.e, for VPN)
    ServerRequest::ReqLevel reqLevel = preTransport ? ServerRequest::FULL : ServerRequest::ONLY_IF_TRANSPORT;

    if (!ServerRequest::MakeRequest(
                        reqLevel,
                        this,
                        sessionInfo,
                        handshakeRequestPath.c_str(),
                        handshakeResponse,
                        m_stopInfo)
        || handshakeResponse.length() <= 0)
    {
        my_print(NOT_SENSITIVE, false, _T("Handshake failed"));
        return false;
    }

    if (!sessionInfo.ParseHandshakeResponse(handshakeResponse.c_str()))
    {
        // If the handshake parsing has failed, something is very wrong.
        my_print(NOT_SENSITIVE, false, _T("%s: ParseHandshakeResponse failed"), __TFUNCTION__);
        MarkServerFailed(sessionInfo.GetServerEntry());
        throw TransportFailed();
    }

    return true;
}


// static
size_t ITransport::AddServerEntries(
        LPCTSTR transportProtocolName,
        const vector<string>& newServerEntryList, 
        const ServerEntry* serverEntry)
{
    ServerList serverList(WStringToUTF8(transportProtocolName).c_str());
    return serverList.AddEntriesToList(newServerEntryList, serverEntry);
}
