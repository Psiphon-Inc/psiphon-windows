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


class IWorkerThread
{
public:
    IWorkerThread();
    virtual ~IWorkerThread();

    // Blocking call. Returns true if worker was successfully started,
    // false otherwise.
    virtual bool Start();

    // Blocking call. Tell the thread to stop and wait for it to do so.
    virtual void Stop();

    // The returned event will be set when the thread stops.
    virtual HANDLE GetStoppedEvent() const;

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
    virtual HANDLE GetSignalStopEvent() const;
    virtual bool IsStopSignalled(bool throwIfSignalled);

    // Called to do worker set-up before going into busy-wait loop
    virtual bool DoStart() = 0;

    // Called from the busy-wait loop every so often.
    virtual bool DoPeriodicCheck() = 0;

    // Called when the implementation should stop and clean up.
    virtual void DoStop() = 0;

    // The actual thread function.
    static DWORD WINAPI Thread(void* object);

protected:
    HANDLE m_thread;
    HANDLE m_startedEvent;
    HANDLE m_stoppedEvent;
    HANDLE m_signalStopEvent;
};

