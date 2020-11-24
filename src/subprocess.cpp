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

#include "stdafx.h"
#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")

#include "subprocess.h"
#include "config.h"
#include "logging.h"
#include "utilities.h"


Subprocess::Subprocess(const tstring& exePath, ISubprocessOutputHandler* outputHandler)
    : m_parentOutputPipe(INVALID_HANDLE_VALUE),
      m_parentInputPipe(INVALID_HANDLE_VALUE)
{
    if (outputHandler == NULL) {
        throw std::exception(__FUNCTION__ ":" STRINGIZE(__LINE__) "outputHandler null");
    }
    ZeroMemory(&m_processInfo, sizeof(m_processInfo));
    m_mutex = CreateMutex(NULL, FALSE, 0);
    m_exePath = exePath;
    m_outputHandler = outputHandler;
}


Subprocess::~Subprocess()
{
    (void)Cleanup();
    CloseHandle(m_mutex);
}


bool Subprocess::SpawnSubprocess(const tstring &commandLineFlags)
{
    AutoMUTEX lock(m_mutex);
    tstringstream commandLine;

    commandLine << m_exePath << commandLineFlags;

    STARTUPINFO startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    startupInfo.dwFlags = STARTF_USESTDHANDLES;

    startupInfo.hStdInput = INVALID_HANDLE_VALUE;
    startupInfo.hStdOutput = INVALID_HANDLE_VALUE;
    startupInfo.hStdError = INVALID_HANDLE_VALUE;
    HANDLE parentInputPipe = INVALID_HANDLE_VALUE;
    m_parentOutputPipe = INVALID_HANDLE_VALUE;
    if (!CreateSubprocessPipes(
        m_parentOutputPipe,
        parentInputPipe,
        startupInfo.hStdInput,
        startupInfo.hStdOutput,
        startupInfo.hStdError))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - CreateSubprocessPipes failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    if (!CreateProcess(
        m_exePath.c_str(),
        (TCHAR*)commandLine.str().c_str(),
        NULL,
        NULL,
        TRUE, // bInheritHandles
#ifdef _DEBUG
        CREATE_NEW_PROCESS_GROUP,
#else
        CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW,
#endif
        NULL,
        NULL,
        &startupInfo,
        &m_processInfo))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - CreateProcess failed (%d)"), __TFUNCTION__, GetLastError());
        CloseHandle(m_parentOutputPipe);
        CloseHandle(startupInfo.hStdInput);
        CloseHandle(startupInfo.hStdOutput);
        CloseHandle(startupInfo.hStdError);
        CloseHandle(parentInputPipe);
        return false;
    }

    // Close the unneccesary handles
    CloseHandle(m_processInfo.hThread);
    m_processInfo.hThread = INVALID_HANDLE_VALUE;

    // NOTE: you need to make sure that no handles to the write, or read,
    // end of the output pipe are maintained in this process or else the
    // pipe will not close when the child process exits and the ReadFile,
    // or WriteFile, will hang.

    bool success = true;

    if (!CloseHandle(startupInfo.hStdInput))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        success = false;
    }
    if (!CloseHandle(startupInfo.hStdOutput))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        success = false;
    }
    if (!CloseHandle(startupInfo.hStdError))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        success = false;
    }
    if (!success) {
        CloseInputPipes();
        if (!CloseHandle(m_parentOutputPipe)) {
            my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        }
        return false;
    }

    WaitForInputIdle(m_processInfo.hProcess, 5000);

    m_parentInputPipe = parentInputPipe;

    return true;
}

HANDLE Subprocess::Process()
{
    return m_processInfo.hProcess;
}

HANDLE Subprocess::ParentInputPipe()
{
    return m_parentInputPipe;
}

bool Subprocess::CloseInputPipes()
{
    AutoMUTEX lock(m_mutex);
    bool success = true;

    if (m_parentInputPipe != NULL && m_parentInputPipe != INVALID_HANDLE_VALUE) {
        if (CloseHandle(m_parentInputPipe)) {
            m_parentInputPipe = INVALID_HANDLE_VALUE;
        }
        else {
            my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __FUNCTION__, __LINE__, GetLastError());
            success = false;
        }
    }
    if (!success && m_parentOutputPipe != NULL && m_parentOutputPipe != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(m_parentOutputPipe)) {
            my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __FUNCTION__, __LINE__, GetLastError());
        }
    }

    return success;
}


bool Subprocess::Cleanup()
{
    AutoMUTEX lock(m_mutex);

    (void)CloseInputPipes();

    // Give the process an opportunity for graceful shutdown, then terminate
    if (m_processInfo.hProcess != NULL
        && m_processInfo.hProcess != INVALID_HANDLE_VALUE)
    {
        // Check if the process has already exited.
        DWORD exitCode;
        if (!GetExitCodeProcess(m_processInfo.hProcess, &exitCode)) {
            my_print(NOT_SENSITIVE, false, _T("%s - GetExitCodeProcess failed (%d)"), __TFUNCTION__, GetLastError());
        }
        else if (exitCode == STILL_ACTIVE) {
            bool stoppedGracefully = false;

            // Allows up to 2 seconds for the subprocess to gracefully shutdown.
            // This gives it an opportunity to exit gracefully. While waiting,
            // continue to consume subprocess output.
            // TODO: AttachConsole/FreeConsole sequence not threadsafe?
            if (AttachConsole(m_processInfo.dwProcessId))
            {
                GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_processInfo.dwProcessId);
                FreeConsole();

                for (int i = 0; i < 2000; i += 10)
                {
                    ConsumeSubprocessOutput();
                    if (WAIT_OBJECT_0 == WaitForSingleObject(m_processInfo.hProcess, 10))
                    {
                        stoppedGracefully = true;
                        break;
                    }
                }
            }
            if (!stoppedGracefully)
            {
                if (!TerminateProcess(m_processInfo.hProcess, 1) ||
                    WAIT_OBJECT_0 != WaitForSingleObject(m_processInfo.hProcess, TERMINATE_PROCESS_WAIT_MS))
                {
                    // Check if the process exited before it could be terminated.
                    if (!GetExitCodeProcess(m_processInfo.hProcess, &exitCode)) {
                        if (GetLastError() != ERROR_INVALID_HANDLE) {
                            my_print(NOT_SENSITIVE, false, _T("%s - GetExitCodeProcess failed (%d)"), __TFUNCTION__, GetLastError());
                            return false;
                        }
                    }
                    else if (exitCode == STILL_ACTIVE) {
                        my_print(NOT_SENSITIVE, false, _T("%s - TerminateProcess failed for process with PID %d: %d"), __TFUNCTION__, m_processInfo.dwProcessId, GetLastError());
                    }
                }
            }
        }
    }

    if (m_processInfo.hProcess != 0
        && m_processInfo.hProcess != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(m_processInfo.hProcess)) {
            my_print(NOT_SENSITIVE, false, _T("%s - CloseHandle failed for process with PID %d: %d"), __TFUNCTION__, m_processInfo.dwProcessId, GetLastError());
        }
    }
    ZeroMemory(&m_processInfo, sizeof(m_processInfo));

    if (m_parentOutputPipe != 0
        && m_parentOutputPipe != INVALID_HANDLE_VALUE)
    {
        if (!CloseHandle(m_parentOutputPipe)) {
            my_print(NOT_SENSITIVE, false, _T("%s - CloseHandle failed for parent pipe to process with PID %d: %d"), __TFUNCTION__, m_processInfo.dwProcessId, GetLastError());
        }
    }
    m_parentOutputPipe = INVALID_HANDLE_VALUE;
    m_parentOutputPipeBuffer.clear();

    return true;
}


void Subprocess::ConsumeSubprocessOutput()
{
    AutoMUTEX lock(m_mutex);
    DWORD bytes_avail = 0;

    // ReadFile will block forever if there's no data to read, so we need
    // to check if there's data available to read first.
    if (!PeekNamedPipe(m_parentOutputPipe, NULL, 0, NULL, &bytes_avail, NULL))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - PeekNamedPipe failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
    }

    if (bytes_avail <= 0)
    {
        return;
    }

    // If there's data available from the pipe, process it.
    unique_ptr<char> buffer(new char[bytes_avail + 1]);

    DWORD num_read = 0;
    if (!ReadFile(m_parentOutputPipe, buffer.get(), bytes_avail, &num_read, NULL))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - ReadFile failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
    }
    buffer.get()[num_read] = '\0';

    // Don't assume we receive complete lines in a read: "Data is written to an anonymous pipe
    // as a stream of bytes. This means that the parent process reading from a pipe cannot
    // distinguish between the bytes written in separate write operations, unless both the
    // parent and child processes use a protocol to indicate where the write operation ends."
    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa365782%28v=vs.85%29.aspx

    m_parentOutputPipeBuffer.append(buffer.get());

    int start = 0;
    while (true)
    {
        int end = m_parentOutputPipeBuffer.find("\n", start);
        if (end == string::npos)
        {
            m_parentOutputPipeBuffer = m_parentOutputPipeBuffer.substr(start);
            break;
        }
        string line = m_parentOutputPipeBuffer.substr(start, end - start);
        m_outputHandler->HandleSubprocessOutputLine(line);
        start = end + 1;
    }
}


DWORD Subprocess::Status()
{
    AutoMUTEX lock(m_mutex);

    if (m_processInfo.hProcess != NULL &&
        m_processInfo.hProcess != INVALID_HANDLE_VALUE)
    {
        // The process handle will be signalled when the process terminates
        DWORD result = WaitForSingleObject(m_processInfo.hProcess, 0);

        if (result == WAIT_TIMEOUT)
        {
            return SUBPROCESS_STATUS_RUNNING;
        }
        else if (result == WAIT_OBJECT_0)
        {
            return SUBPROCESS_STATUS_EXITED;
        }
        else
        {
            std::stringstream s;
            s << __FUNCTION__ << ": WaitForSingleObject failed (" << result << ", " << GetLastError() << ")";
            throw Error(s.str().c_str());
        }
    }

    return SUBPROCESS_STATUS_NO_PROCESS;
}
