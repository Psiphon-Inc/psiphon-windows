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
#include "utilities.h"


IWorkerThread::IWorkerThread()
    : m_thread(0),
      m_externalStopSignalFlag(0),
      m_internalSignalStopFlag(false),
      m_synchronizedExitCounter(0)
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
}

IWorkerThread::~IWorkerThread()
{
    // Subclasses MUST call IWorkerThread::Stop() in their destructor.
    assert(m_thread == 0);

    CloseHandle(m_startedEvent);
    CloseHandle(m_stoppedEvent);
}

HANDLE IWorkerThread::GetStoppedEvent() const
{
    return m_stoppedEvent;
}

const vector<const bool*>& IWorkerThread::GetSignalStopFlags() const
{
    return m_signalStopFlags;
}

bool IWorkerThread::Start(
                    const bool& externalStopSignalFlag, 
                    ReferenceCounter* synchronizedExitCounter)
{
    assert(m_thread == 0);
    assert(m_externalStopSignalFlag == 0);

    ResetEvent(m_startedEvent);
    ResetEvent(m_stoppedEvent);
    
    m_externalStopSignalFlag = &externalStopSignalFlag;
    m_internalSignalStopFlag = false;
    m_synchronizedExitCounter = synchronizedExitCounter;

    m_signalStopFlags.clear();
    m_signalStopFlags.push_back(&m_internalSignalStopFlag);
    m_signalStopFlags.push_back(m_externalStopSignalFlag);

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

    if (!started)
    {
        Stop();
    }

    return started;
}

void IWorkerThread::Stop()
{
    m_internalSignalStopFlag = true;

    if (m_thread != INVALID_HANDLE_VALUE && m_thread != 0)
    {
        (void)WaitForSingleObject(m_thread, INFINITE);
    }

    m_thread = 0;

    m_externalStopSignalFlag = 0;
}

bool IWorkerThread::IsRunning() const
{
    bool started = (WaitForSingleObject(m_startedEvent, 0) == WAIT_OBJECT_0);
    bool stopped = (WaitForSingleObject(m_stoppedEvent, 0) == WAIT_OBJECT_0);
    return started && !stopped;
}

// static
DWORD WINAPI IWorkerThread::Thread(void* object)
{
    IWorkerThread* _this = (IWorkerThread*)object;

    if (_this->m_synchronizedExitCounter)
    {
        _this->m_synchronizedExitCounter->Increment();
    }

    // We only attempt a synchronized exit on user cancel (i.e., on a nice,
    // clean exit).
    bool doSynchronizedExit = false;

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
            Sleep(100);

            if (TestBoolArray(_this->GetSignalStopFlags()))
            {
                // Stop request signalled. Need to stop now.
                _this->StopImminent();
                doSynchronizedExit = true;
                break;
            }
            else
            {
                if (!_this->DoPeriodicCheck())
                {
                    // Implementation indicates that we need to stop.
                    break;
                }
            }
        }
    }
    catch(...)
    {
        // Fall through and exit cleanly
    }

    if (_this->m_synchronizedExitCounter)
    {
        _this->m_synchronizedExitCounter->Decrement();
    }

    if (doSynchronizedExit)
    {
        // Wait for all related threads to release the exit counter before 
        // stopping completely.
        while (_this->m_synchronizedExitCounter->Check())
        {
            Sleep(100);
        }
    }

    _this->DoStop();
    SetEvent(_this->m_stoppedEvent);

    return 0;
}
