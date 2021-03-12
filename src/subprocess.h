/*
 * Copyright (c) 2020, Psiphon Inc.
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

#include "worker_thread.h"

// Subprocess is running
#define SUBPROCESS_STATUS_RUNNING    0x0L

// Subprocess has signalled (exited)
#define SUBPROCESS_STATUS_EXITED     (1L << 0)

// No subprocess is running
#define SUBPROCESS_STATUS_NO_PROCESS (1L << 1)


class ISubprocessOutputHandler
{
public:
    /**
    Called for each line of data read from the subprocess when
    ConsumeSubprocessOutput is invoked on a Subprocess instance.
    See Subprocess::ConsumeSubprocessOutput.
    */
    virtual void HandleSubprocessOutputLine(const string& line) = 0;
};

/**
Subprocess provides functionality around launching an executable as a
subprocess. This includes handling output written by the subproccess
to stdout, querying the state of the running process and managing its
lifecycle. The provided methods are thread safe.
*/
class Subprocess
{
public:
    /**
    Initialize a new instance. Throws std::exception if outputHandler is null.
    */
    Subprocess(const tstring& exePath, ISubprocessOutputHandler* outputHandler);
    virtual ~Subprocess();

    /**
    Spawns the subprocess with the given command line flags. If successful,
    then the child input pipe and parent input pipe will have been opened,
    and should be closed with CloseInputPipes(), as soon as they are not
    needed. Returns true if the subprocess was successfully started,
    otherwise returns false.
    */
    virtual bool SpawnSubprocess(const tstring& commandLineFlags);

    /**
    Reads stdout of the child process and calls HandleSubprocessOutputLine,
    on the provided SubprocessOutputHandler, once for each line of newline
    delimited output data read, until there is no more data to be read.
    */
    virtual void ConsumeSubprocessOutput();

    /**
    Subprocess status. Possible values are defined by the constants
    SUBPROCESS_STATUS_{RUNNING, EXITED, NO_PROCESS}.
    Throws Error in the event of a fatal system error.
    */
    virtual DWORD Status();

    /**
    Returns a handle to the subprocess. Only returns a valid handle when
    Status() == SUBPROCESS_STATUS_RUNNING.
    */
    virtual HANDLE Process();

    /**
    Returns a handle to stdin of the subprocess. Only returns a valid handle
    when Status() == SUBPROCESS_STATUS_RUNNING.
    */
    virtual HANDLE ParentInputPipe();

    /**
    Close child input pipe and parent input pipe. Should be called once
    these are pipes are not required. See SpawnSubprocess(..). This
    function can take several seconds to complete.
    Returns true if the pipes were closed successfully, otherwise returns
    false.
    */
    virtual bool CloseInputPipes();

    // Indicates a fatal system error
    class Error
    {
    public:
        Error(const TCHAR* msg = NULL) { if (msg) m_msg = msg; }
        Error(const char* msg) { if (msg) m_msg = UTF8ToWString(msg); }
        tstring GetMessage() { return m_msg; }
    private:
        tstring m_msg;
    };

protected:
    /**
    Cleanup should be invoked to cleanup any underlying resources and will
    terminate the subprocess if it is still running.
    Returns true if the cleanup succeeded, otherwise false.
    */
    virtual bool Cleanup();

    tstring m_exePath;
    PROCESS_INFORMATION m_processInfo;
    HANDLE m_parentInputPipe;
    HANDLE m_parentOutputPipe;
    string m_parentOutputPipeBuffer;
    HANDLE m_mutex;
    ISubprocessOutputHandler* m_outputHandler;
};
