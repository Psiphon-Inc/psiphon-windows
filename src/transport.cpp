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
#include "stopsignal.h"
#include "server_request.h"
#include "embeddedvalues.h"
#include "config.h"


/******************************************************************************
 ITransport
******************************************************************************/

ITransport::ITransport()
    : m_systemProxySettings(NULL),
      m_serverList(GetTransportProtocolName().c_str())
{
}

bool ITransport::ServerWithCapabilitiesExists() const
{
    *** check member list (load if not loaded)

    ServerEntries entries = serverList.GetList();

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
                    WorkerThreadSynch* workerThreadSynch)
{
    m_systemProxySettings = systemProxySettings;

    assert(m_systemProxySettings);

    if (!IWorkerThread::Start(stopInfo, workerThreadSynch))
    {
        throw TransportFailed();
    }
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
    catch(...)
    {
        return false;
    }

    my_print(NOT_SENSITIVE, false, _T("%s successfully connected."), GetTransportDisplayName().c_str());

    return true;
}

void ITransport::StopImminent()
{
}

void ITransport::DoStop(bool cleanly)
{
    Cleanup();
    m_systemProxySettings = NULL;

    my_print(NOT_SENSITIVE, false, _T("%s disconnected."), GetTransportDisplayName().c_str());
}

bool ITransport::IsConnected() const
{
    return IsRunning();
}


void ITransport::MarkServerFailed(const ServerEntry& serverEntry)
{
    m_serverList.MarkServerFailed(serverEntry);
}


void ITransport::MarkServerSucceeded(const ServerEntry& serverEntry)
{
    m_serverList.MoveEntryToFront(serverEntry);
}


tstring ITransport::GetHandshakeRequestPath(const SessionInfo& sessionInfo)
{
    tstring handshakeRequestPath;
    handshakeRequestPath = tstring(HTTP_HANDSHAKE_REQUEST_PATH) + 
                           _T("?client_session_id=") + NarrowToTString(sessionInfo.GetClientSessionID()) +
                           _T("&propagation_channel_id=") + NarrowToTString(PROPAGATION_CHANNEL_ID) +
                           _T("&sponsor_id=") + NarrowToTString(SPONSOR_ID) +
                           _T("&client_version=") + NarrowToTString(CLIENT_VERSION) +
                           _T("&server_secret=") + NarrowToTString(sessionInfo.GetWebServerSecret()) +
                           _T("&relay_protocol=") + GetTransportProtocolName();

    // Include a list of known server IP addresses in the request query string as required by /handshake
    ServerEntries serverEntries = GetServerEntries();
    for (ServerEntryIterator ii = serverEntries.begin(); ii != serverEntries.end(); ++ii)
    {
        handshakeRequestPath += _T("&known_server=");
        handshakeRequestPath += NarrowToTString(ii->serverAddress);
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
