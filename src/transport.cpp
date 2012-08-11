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
    : m_systemProxySettings(NULL)
{
}

void ITransport::Connect(
                    SessionInfo sessionInfo, 
                    SystemProxySettings* systemProxySettings,
                    const StopInfo& stopInfo,
                    WorkerThreadSynch* workerThreadSynch)
{
    m_sessionInfo = sessionInfo;
    m_systemProxySettings = systemProxySettings;
    assert(m_systemProxySettings);

    if (!IWorkerThread::Start(stopInfo, workerThreadSynch))
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

void ITransport::StopImminent()
{
}

void ITransport::DoStop(bool cleanly)
{
    Cleanup();
    m_systemProxySettings = 0;

    my_print(false, _T("%s disconnected."), GetTransportDisplayName().c_str());
}

bool ITransport::IsConnected() const
{
    return IsRunning();
}
