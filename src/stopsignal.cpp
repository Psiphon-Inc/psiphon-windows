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

Implements global stop-signals to indicate when various types of processing
should stop. 

Requirements:

1. It must be possible to set and report different stop reasons. 
   - E.g., we need to be able to indicate whether the user has indicated that
     he'd like to close the app vs. the transport unexpectedly disconnected.
     Different actions will result in each case.

1a. It must be possible for code to react to different subsets of stop reasons.

2. Stop reasons must be provided as both return codes and exceptions. We don't
   always want to throw exceptions in stop conditions, but sometimes we do.

Complications to remember:
- Clearing/resetting stop conditions should probably be done in a fine-grained manner.
- Should there be a separate stop reason for a split-tunnel switch? The 
  transport gets pulled down, but then immediately put back up.

*/


#include "stdafx.h"
#include "stopsignal.h"
#include "psiclient.h"
#include "utilities.h"


/***********************************************************************
 StopSignal
 */

StopSignal::StopSignal() 
{ 
    m_stop = STOP_REASON_NONE;

    // Note that because our stop signal is a simple primitive type, and we're
    // only doing simple sets or simple checks (but not check-and-set), then we
    // don't really need to use a mutex. But a) this simple functionality might 
    // change in the future; b) using a mutex isn't a lot of overhead; c) using
    // a mutex makes it clear that this is threadsafe.
    m_mutex = CreateMutex(NULL, FALSE, 0);
}

StopSignal::~StopSignal()
{
    CloseHandle(m_mutex);
    m_mutex = 0;
}

DWORD StopSignal::CheckSignal(DWORD reasons, bool throwIfTrue/*=false*/) const
{
    AutoMUTEX lock(m_mutex);
    if (throwIfTrue && (reasons & m_stop)) 
    {
        ThrowSignalException(reasons & m_stop);
    }
    return (reasons & m_stop);
}

void StopSignal::SignalStop(DWORD reason)
{
    AutoMUTEX lock(m_mutex);
    m_stop = m_stop | reason;
}

void StopSignal::ClearStopSignal(DWORD reason)
{
    AutoMUTEX lock(m_mutex);
    m_stop = m_stop & ~reason;
}

// static
void StopSignal::ThrowSignalException(DWORD reason)
{
    // `reason` may match more than one reason, but we'll just throw the first match

    if (reason & STOP_REASON_USER_DISCONNECT)
    {
        throw StopSignal::UserDisconnectException();
    }
    else if (reason & STOP_REASON_EXIT)
    {
        throw StopSignal::ExitStopException();
    }
    else if (reason & STOP_REASON_UNEXPECTED_DISCONNECT)
    {
        throw StopSignal::UnexpectedDisconnectStopException();
    }

    throw StopSignal::NoStopException();
}


/***********************************************************************
 GlobalStopSignal
 */

GlobalStopSignal* GlobalStopSignal::MInstance = 0;

GlobalStopSignal::GlobalStopSignal() 
{ 
    atexit(&CleanUp); 
}

GlobalStopSignal::~GlobalStopSignal()
{
}

GlobalStopSignal& GlobalStopSignal::Instance() 
{ 
    if (MInstance == 0) MInstance = new GlobalStopSignal(); 
    return *MInstance; 
}

void GlobalStopSignal::CleanUp() 
{ 
    delete MInstance; 
    MInstance = 0;
}
