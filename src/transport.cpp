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


/******************************************************************************
 ITransport
******************************************************************************/

ITransport::ITransport()
    : m_systemProxySettings(NULL),
      m_failedServerEntries(NULL),
      m_chosenSessionInfoIndex(-1)
{
}

bool ITransport::ServerWithCapabilitiesExists(ServerList& serverList) const
{
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
                    const vector<SessionInfo>& sessionInfo, 
                    SystemProxySettings* systemProxySettings,
                    const StopInfo& stopInfo,
                    WorkerThreadSynch* workerThreadSynch,
                    int& o_chosenSessionInfoIndex,
                    ServerEntries& o_failedServerEntries)
{
    o_chosenSessionInfoIndex = -1;
    m_sessionInfo = sessionInfo;
    m_chosenSessionInfoIndex = -1;
    m_systemProxySettings = systemProxySettings;

    // Do *not* clear o_failedServerEntries -- it might already contain important entries.
    m_failedServerEntries = &o_failedServerEntries;
 
    assert(m_systemProxySettings);
    assert(sessionInfo.size() > 0);

    // There's no reason for the number of supplied SessionInfo objects to be
    // greater than the number that can be used in a single connection attempt.
    assert(sessionInfo.size() <= GetMultiConnectCount());

    if (!IWorkerThread::Start(stopInfo, workerThreadSynch))
    {
        throw TransportFailed();
    }

    assert(m_chosenSessionInfoIndex >= 0 && m_chosenSessionInfoIndex < (signed)sessionInfo.size());
    o_chosenSessionInfoIndex = m_chosenSessionInfoIndex;
}

void ITransport::UpdateSessionInfo(const SessionInfo& sessionInfo)
{
    assert(m_chosenSessionInfoIndex >= 0 && m_chosenSessionInfoIndex < (signed)m_sessionInfo.size());
    m_sessionInfo[m_chosenSessionInfoIndex] = sessionInfo;
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
    m_failedServerEntries = NULL;

    my_print(NOT_SENSITIVE, false, _T("%s disconnected."), GetTransportDisplayName().c_str());
}

bool ITransport::IsConnected() const
{
    return IsRunning();
}


void ITransport::AddFailedServer(const SessionInfo& sessionInfo)
{
    assert(m_failedServerEntries);
    if (!m_failedServerEntries)
    {
        return;
    }

    // Make sure the entry isn't already marked as failed
    for (ServerEntries::const_iterator it = m_failedServerEntries->begin();
            it != m_failedServerEntries->end();
            ++it)
    {
        if (it->serverAddress == sessionInfo.GetServerEntry().serverAddress)
        {
            return;
        }
    }

    m_failedServerEntries->push_back(sessionInfo.GetServerEntry());
}

void ITransport::AddFailedServers(const vector<SessionInfo>& sessionInfo)
{
    for (vector<SessionInfo>::const_iterator it = sessionInfo.begin();
         it != sessionInfo.end();
         ++it)
    {
        AddFailedServer(*it);
    }
}
