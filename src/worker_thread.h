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


class ReferenceCounter
{
public:
    ReferenceCounter() { Reset(); }

    void Reset() { m_counter = 0; }
    void Increment() { InterlockedIncrement(&m_counter); }
    void Decrement() { InterlockedDecrement(&m_counter); }

    // Returns false when there are no more references.
    bool Check() const { return m_counter == 0; }

private:
    LONG m_counter;
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
        const bool& externalStopSignalFlag, 
        ReferenceCounter* synchronizedExitCounter);

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
        Error(const char* msg) { if (msg) m_msg=NarrowToTString(msg); } 
        tstring GetMessage() { return m_msg; }
    private:
        tstring m_msg;
    };

protected:
    const vector<const bool*>& GetSignalStopFlags() const;

    // Called to do worker set-up before going into busy-wait loop
    virtual bool DoStart() = 0;

    // Called from the busy-wait loop every so often.
    virtual bool DoPeriodicCheck() = 0;

    // Called before stop is full processed. Must not take any destructive
    // actions.
    virtual void StopImminent() = 0;

    // Called when the implementation should stop and clean up.
    virtual void DoStop() = 0;

    // The actual thread function.
    static DWORD WINAPI Thread(void* object);

protected:
    HANDLE m_thread;
    HANDLE m_startedEvent;
    HANDLE m_stoppedEvent;
    HANDLE m_mutex;

    const bool* m_externalStopSignalFlag;
    bool m_internalSignalStopFlag;
    vector<const bool*> m_signalStopFlags;

    // We will sometimes want multiple related threads to do some pre-stop
    // work before we stop all threads. I.e., we want LocalProxy to do a final
    // /status request before the transport is torn down. If the same 
    // ReferenceCounter is passed to multiple threads, they will wait until
    // all threads have processed StopImminent() before they shut down all 
    // the way.
    // Note that these steps are only followed on a user-signalled stop.
    ReferenceCounter* m_synchronizedExitCounter;
};

