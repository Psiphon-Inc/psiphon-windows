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
#include "logging.h"
#include "utilities.h"
#include "psiclient.h"
#include "stopsignal.h"

/*****************
 * WorkerThreadStopSignal
 *****************/

/*
IWorkerThread needs to mix in its own stop signals to the one that's passed in.
This custom stop signal class will encapsulate that.
*/
class WorkerThreadStopSignal : public StopSignal
{
public:
    // Note that ownership of parentStopSignal is *not* taken (won't be deleted)
    WorkerThreadStopSignal(StopSignal* parentStopSignal, const bool& additionalStopFlag);

    virtual DWORD CheckSignal(DWORD reasons, bool throwIfTrue=false) const;
    virtual void SignalStop(DWORD reason);
    virtual void ClearStopSignal(DWORD reason);

private:
    StopSignal* m_parentStopSignal;
    const bool& m_additionalStopFlag;
};

WorkerThreadStopSignal::WorkerThreadStopSignal(
                            StopSignal* parentStopSignal, 
                            const bool& additionalStopFlag)
    : m_parentStopSignal(parentStopSignal),
      m_additionalStopFlag(additionalStopFlag)
{
}

DWORD WorkerThreadStopSignal::CheckSignal(DWORD reasons, bool throwIfTrue/*=false*/) const
{
    if (throwIfTrue && m_additionalStopFlag)
    {
        throw IWorkerThread::Abort();
    }
    
    // TODO: Maybe this should throw Abort() if true and not whatever CheckSignal throws?
    return m_parentStopSignal->CheckSignal(reasons, throwIfTrue) 
           || m_additionalStopFlag;
}

void WorkerThreadStopSignal::SignalStop(DWORD reason)
{
    m_parentStopSignal->SignalStop(reason);
}

void WorkerThreadStopSignal::ClearStopSignal(DWORD reason)
{
    StopSignal::ClearStopSignal(reason);
    m_parentStopSignal->ClearStopSignal(reason);
}


/*****************
 * IWorkerThread
 *****************/

IWorkerThread::IWorkerThread()
    : m_thread(0),
      m_internalSignalStopFlag(false),
      m_workerThreadSynch(0)
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

    if (m_startedEvent == NULL || m_stoppedEvent == NULL)
    {
        throw std::exception(__FUNCTION__ ":" STRINGIZE(__LINE__) " CreateEvent failed");
    }
}

IWorkerThread::~IWorkerThread()
{
    // Subclasses MUST call IWorkerThread::Stop() in their destructor.
    assert(m_thread == 0);

    if (m_stopInfo.stopSignal) 
    {
        delete m_stopInfo.stopSignal;
        m_stopInfo.stopSignal = 0;
    }

    CloseHandle(m_startedEvent);
    CloseHandle(m_stoppedEvent);
}

HANDLE IWorkerThread::GetStoppedEvent() const
{
    return m_stoppedEvent;
}

bool IWorkerThread::Start(
                       const StopInfo& stopInfo,
                       WorkerThreadSynch* workerThreadSynch)
{
    assert(m_thread == 0);

    ResetEvent(m_startedEvent);
    ResetEvent(m_stoppedEvent);
    
    m_internalSignalStopFlag = false;
    m_workerThreadSynch = workerThreadSynch;

    if (m_stopInfo.stopSignal) 
    {
        delete m_stopInfo.stopSignal;
        m_stopInfo.stopSignal = 0;
    }

    m_stopInfo = StopInfo(
                    new WorkerThreadStopSignal(stopInfo.stopSignal, m_internalSignalStopFlag),
                    stopInfo.stopReasons);

    // Throws if true
    m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons, true);

    m_thread = CreateThread(0, 0, IWorkerThread::Thread, (void*)this, 0, 0);
    if (!m_thread)
    {
        Stop();

        std::stringstream s;
        s << typeid(*this).name() << "::IWorkerThread::Start: CreateThread failed (" << GetLastError() << ")";
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
        s << typeid(*this).name() << "::IWorkerThread::Start: WaitForMultipleObjects failed (" << waitReturn << ", " << GetLastError() << ")";
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

    if (m_stopInfo.stopSignal) 
    {
        delete m_stopInfo.stopSignal;
        m_stopInfo.stopSignal = 0;
    }
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

    unsigned int tid = GetCurrentThreadId();
    srand((unsigned)time(0) + tid);

    // See the comments in the WorkerThreadSynch code below for info about the
    // thread synchronization.

    if (_this->m_workerThreadSynch)
    {
        _this->m_workerThreadSynch->ThreadStarting();
    }

    bool stoppingCleanly = false;

    // Not allowed to throw out of the thread without cleaning up.
    try
    {
        if (_this->m_stopInfo.stopSignal->CheckSignal(_this->m_stopInfo.stopReasons, false)
            || (_this->m_workerThreadSynch && _this->m_workerThreadSynch->IsThreadStopping()))
        {
            throw Abort();
        }

        bool success = _this->DoStart();

        if (success)
        {
            SetEvent(_this->m_startedEvent);
        }    
    
        while (success)
        {
            Sleep(100);

            if (_this->m_stopInfo.stopSignal->CheckSignal(_this->m_stopInfo.stopReasons, false)
                || (_this->m_workerThreadSynch && _this->m_workerThreadSynch->IsThreadStopping()))
            {
                // Stop request signalled. Need to stop now.
                stoppingCleanly = true;
                my_print(NOT_SENSITIVE, true, _T("%S::%s: CheckSignal or IsThreadStopping returned true"), typeid(*_this).name(), __TFUNCTION__);
                break;
            }
            else
            {
                if (!_this->DoPeriodicCheck())
                {
                    // Implementation indicates that we need to stop.
                    my_print(NOT_SENSITIVE, true, _T("%S::%s: DoPeriodicCheck returned false"), typeid(*_this).name(), __TFUNCTION__);
                    break;
                }
            }
        }
    }
    catch(...)
    {
        // Fall through and exit cleanly
    }

    // Allow all synched threads to do clean stops, if possible.
    if (_this->m_workerThreadSynch)
    {
        _this->m_workerThreadSynch->ThreadStoppingCleanly(stoppingCleanly);

        // If we're stopping cleanly, then continue the clean exit sequence.
        // But if we're not, then just get out of here.
        if (stoppingCleanly)
        {
            my_print(NOT_SENSITIVE, true, _T("%S::%s: Waiting for all threads to indicate clean stop"), typeid(*_this).name(), __TFUNCTION__);
            if (_this->m_workerThreadSynch->BlockUntil_AllThreadsStoppingCleanly())
            {
                my_print(NOT_SENSITIVE, true, _T("%S::%s: All threads indicated clean stop"), typeid(*_this).name(), __TFUNCTION__);
                
                _this->StopImminent();

                my_print(NOT_SENSITIVE, true, _T("%S::%s: Waiting for all threads to indicate ready to stop"), typeid(*_this).name(), __TFUNCTION__);
                _this->m_workerThreadSynch->ThreadReadyForStop();
                _this->m_workerThreadSynch->BlockUntil_AllThreadsReadyToStop();
            }
            // If some other thread has an un-clean stop, we need to bail ASAP.
        }
    }

    _this->DoStop(stoppingCleanly);
    SetEvent(_this->m_stoppedEvent);

    return 0;
}


/***********************************
class WorkerThreadSynch
*/

/*
With respect to synchronization between worker threads, this is the flow:
- Threads indicate to the synch object that they have started.
- When a thread leaves the busy-wait loop, it indicates if it's stopping 
  cleanly (i.e., due to user-cancel) or not.
- Then each thread waits until the other synched threads have set their 
  clean-flags.
- If the clean-flags are all set, threads do graceful-stop work. When the 
  graceful-stop work is done, threads will indicate.
- When all threads have indicated graceful-stop work is done (or if the 
  clean-flags weren't set in the first place), then threads will stop.
*/

WorkerThreadSynch::WorkerThreadSynch()
{
    m_mutex = CreateMutex(NULL, FALSE, 0);
    Reset();
}

WorkerThreadSynch::~WorkerThreadSynch()
{
    CloseHandle(m_mutex);
}

void WorkerThreadSynch::Reset()
{
    m_threadsStartedCounter = 0;
    m_threadsReadyToStopCounter = 0;
    m_threadCleanStops.clear();
}

void WorkerThreadSynch::ThreadStarting()
{
    AutoMUTEX lock(m_mutex);
    m_threadsStartedCounter++;
}

void WorkerThreadSynch::ThreadStoppingCleanly(bool clean)
{
    AutoMUTEX lock(m_mutex);
    assert(m_threadCleanStops.size() < m_threadsStartedCounter);
    m_threadCleanStops.push_back(clean);
}

bool WorkerThreadSynch::IsThreadStopping() const
{
    return m_threadCleanStops.size() > 0;
}

// Does an early return if there's a single unclean stop indicated.
bool WorkerThreadSynch::BlockUntil_AllThreadsStoppingCleanly()
{
    bool allThreadsReporting = false;
    while (!allThreadsReporting)
    {
        // Keep the mutex lock in a different scope than the sleep.
        {
            AutoMUTEX lock(m_mutex);
            allThreadsReporting = 
                (m_threadCleanStops.size() == m_threadsStartedCounter);

            vector<bool>::const_iterator it;
            for (it = m_threadCleanStops.begin(); it != m_threadCleanStops.end(); it++)
            {
                if (*it == false)
                {
                    return false;
                }
            }
        }

        if (!allThreadsReporting) Sleep(100);
    }

    return true;
}

void WorkerThreadSynch::ThreadReadyForStop()
{
    AutoMUTEX lock(m_mutex);
    assert(m_threadsReadyToStopCounter < m_threadsStartedCounter);
    m_threadsReadyToStopCounter++;
}

void WorkerThreadSynch::BlockUntil_AllThreadsReadyToStop()
{
    bool allThreadsReporting = false;
    while (!allThreadsReporting)
    {
        // Keep the mutex lock in a different scope than the sleep.
        {
            AutoMUTEX lock(m_mutex);
            allThreadsReporting = 
                (m_threadsReadyToStopCounter == m_threadsStartedCounter);
        }

        if (!allThreadsReporting)
        {
            Sleep(100);
        }
    }

    // All threads reporting.
}
