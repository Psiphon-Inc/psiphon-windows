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

#include "stdafx.h"
#include "worker_thread.h"


IWorkerThread::IWorkerThread()
    : m_thread(0)
{
    m_startedEvent = CreateEvent(
                        NULL, 
                        TRUE,  // manual reset
                        FALSE, // initial state
                        0);

    m_stoppedEvent = CreateEvent(
                        NULL, 
                        TRUE,  // manual reset
                        TRUE,  // initial state should be SET
                        0);

    m_signalStopEvent = CreateEvent(
                        NULL, 
                        TRUE,  // manual reset
                        FALSE, // initial state
                        0);
}

IWorkerThread::~IWorkerThread()
{
    Stop();
    CloseHandle(m_startedEvent);
    CloseHandle(m_stoppedEvent);
    CloseHandle(m_signalStopEvent);
}

HANDLE IWorkerThread::GetStoppedEvent() const
{
    return m_stoppedEvent;
}

HANDLE IWorkerThread::GetSignalStopEvent() const
{
    return m_signalStopEvent;
}

bool IWorkerThread::IsStopSignalled(bool throwIfSignalled)
{
    DWORD result = WaitForSingleObject(GetSignalStopEvent(), 0);
    bool signalled = (result == WAIT_OBJECT_0);
    if (throwIfSignalled && signalled)
    {
        throw Abort();
    }
    return signalled;
}

bool IWorkerThread::Start()
{
    _ASSERT(m_thread == 0);

    ResetEvent(m_startedEvent);
    ResetEvent(m_stoppedEvent);
    ResetEvent(m_signalStopEvent);

    m_thread = CreateThread(0, 0, IWorkerThread::Thread, (void*)this, 0, 0);
    if (!m_thread)
    {
        Stop();

        std::stringstream s;
        s << "IWorkerThread::Start: CreateThread failed (" << GetLastError() << ")";
        throw Error(s.str().c_str());
    }

    HANDLE events[] = { m_startedEvent, m_stoppedEvent };
    size_t eventsCount = sizeof(events)/sizeof(*events);

    DWORD waitReturn = WaitForMultipleObjects(
                            eventsCount, 
                            events, 
                            FALSE, // wait for any event
                            INFINITE);

    if (waitReturn > (WAIT_OBJECT_0 + eventsCount))
    {
        Stop();

        std::stringstream s;
        s << "IWorkerThread::Start: WaitForMultipleObjects failed (" << waitReturn << ", " << GetLastError() << ")";
        throw Error(s.str().c_str());
    }

    bool started = (waitReturn == WAIT_OBJECT_0);
    return started;
}

void IWorkerThread::Stop()
{
    SetEvent(m_signalStopEvent);

    if (m_thread != INVALID_HANDLE_VALUE && m_thread != 0)
    {
        (void)WaitForSingleObject(m_thread, INFINITE);
    }

    m_thread = 0;
}

// static
DWORD WINAPI IWorkerThread::Thread(void* object)
{
    IWorkerThread* _this = (IWorkerThread*)object;

    // Not allowed to throw out of the thread without cleaning up.
    try
    {
        bool success = _this->DoStart();

        if (success)
        {
            SetEvent(_this->m_startedEvent);
        }    
    
        while (success)
        {
            DWORD waitResult = WaitForSingleObject(_this->m_signalStopEvent, 100);

            if (waitResult == WAIT_OBJECT_0)
            {
                // m_signalStopEvent set. Need to stop.
                _this->DoStop();
                break;
            }
            else if (waitResult == WAIT_TIMEOUT)
            {
                if (!_this->DoPeriodicCheck())
                {
                    // Implementation indicates that we need to stop.
                    _this->DoStop(); // possibly a no-op, but for completeness
                    break;
                }
            }
            else 
            {
                // An error occurred in the wait call
                _this->DoStop();
                break;
            }
        }
    }
    catch(...)
    {
        // Fall through and exit cleanly
    }

    _this->DoStop();
    SetEvent(_this->m_stoppedEvent);

    return 0;
}
