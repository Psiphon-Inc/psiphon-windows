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

#pragma once

#include "stopsignal.h"


class WorkerThreadSynch
{
public:
    WorkerThreadSynch();
    ~WorkerThreadSynch();

    void Reset();

protected:
    friend class IWorkerThread;

    void ThreadStarting();
    
    void ThreadStoppingCleanly(bool clean);
    bool IsThreadStopping() const;
    bool BlockUntil_AllThreadsStoppingCleanly();

    void ThreadReadyForStop();
    void BlockUntil_AllThreadsReadyToStop();

private:
    HANDLE m_mutex;
    unsigned int m_threadsStartedCounter;
    unsigned int m_threadsReadyToStopCounter;
    vector<bool> m_threadCleanStops;
};


class IWorkerThread
{
public:
    IWorkerThread();
    virtual ~IWorkerThread();

    // Blocking call. Returns true if worker was successfully started,
    // false otherwise.
    // synchronizedExitCounter can be null if not needed.
    virtual bool Start(
        const StopInfo& stopInfo,
        WorkerThreadSynch* workerThreadSynch);

    // Blocking call. Tell the thread to stop and wait for it to do so.
    // Implementing classes MUST call this from their destructor.
    virtual void Stop();

    // The returned event will be set when the thread stops.
    virtual HANDLE GetStoppedEvent() const;

    bool IsRunning() const;

    //
    // Exception classes
    //
    // Generally speaking, any of these may be thrown at any time. 
    // (Except in const members?)
    //
    // Indicates that a stop event was signalled
    class Abort { };

    // Indicates a fatal system error
    class Error 
    { 
    public:
        Error(const TCHAR* msg=NULL) { if (msg) m_msg=msg; } 
        Error(const char* msg) { if (msg) m_msg=UTF8ToWString(msg); } 
        tstring GetMessage() { return m_msg; }
    private:
        tstring m_msg;
    };

protected:
    // Called to do worker set-up before going into busy-wait loop
    virtual bool DoStart() = 0;

    // Called from the busy-wait loop every so often.
    virtual bool DoPeriodicCheck() = 0;

    // Called before stop is full processed. Must not take any destructive
    // actions.
    virtual void StopImminent() = 0;

    // Called when the implementation should stop and clean up.
    // `cleanly` indicates whether the stop is clean (expected, triggered)
    // or not (something died).
    virtual void DoStop(bool cleanly) = 0;

    // The actual thread function.
    static DWORD WINAPI Thread(void* object);

protected:
    HANDLE m_thread;
    HANDLE m_startedEvent;
    HANDLE m_stoppedEvent;

    bool m_internalSignalStopFlag;
    StopInfo m_stopInfo;

    WorkerThreadSynch* m_workerThreadSynch;
};

