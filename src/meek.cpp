/*
 * Copyright (c) 2014, Psiphon Inc.
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
#include "meek.h"
#include "utilities.h"
#include "psiclient.h"



#define MEEK_CONNECTION_TIMEOUT_SECONDS   20
#define MEEK_EXE_NAME                     _T("psiphon3-meek.exe")


Meek::Meek() :
     m_meekPipe(NULL),
     m_meekLocalPort(0)
{
    ZeroMemory(&m_meekProcessInfo, sizeof(m_meekProcessInfo));

    m_mutex = CreateMutex(NULL, FALSE, 0);
    if (m_mutex == NULL)
    {
        throw std::exception(__FUNCTION__ ":" STRINGIZE(__LINE__) " CreateMutex failed");
    }
}


Meek::~Meek(void)
{
        try
    {
        IWorkerThread::Stop();

        Cleanup();
    }
    catch (...)
    {
        // Cleanup might throw, but we're in the destructor, so just swallow it.
    }

    if (m_mutex != NULL)
    {
        CloseHandle(m_mutex);
    }
}

bool Meek::DoStart()
{
    if (m_meekPath.size() == 0)
    {
        if (!ExtractExecutable(IDR_MEEK_EXE, MEEK_EXE_NAME, m_meekPath))
        {
            return false;
        }
    }

    // Ensure we start from a disconnected/clean state
    Cleanup();
    
    if (!StartMeek())
    {
        Cleanup();
        return false;
    }
    return true;
}

void Meek::DoStop(bool cleanly)
{
    if (!cleanly) 
    {
        m_stopInfo.stopSignal->SignalStop(STOP_REASON_UNEXPECTED_DISCONNECT);
    }

    Cleanup();
}

void Meek::StopImminent()
{
    //really nothing to do here
}

void Meek::LogOutput()
{
    DWORD bytes_avail = 0;

    // ReadFile will block forever if there's no data to read, so we need
    // to check if there's data available to read first.
    if (!PeekNamedPipe(m_meekPipe, NULL, 0, NULL, &bytes_avail, NULL))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - PeekNamedPipe failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
    }

    // If there's data available from the Polipo pipe, process it.
    if (bytes_avail > 0)
    {
        char* buffer = new char[bytes_avail+1];
        DWORD num_read = 0;
        if (!ReadFile(m_meekPipe, buffer, bytes_avail, &num_read, NULL))
        {
            my_print(NOT_SENSITIVE, false, _T("%s:%d - ReadFile failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        }
        buffer[bytes_avail] = '\0';

        my_print(NOT_SENSITIVE, true, _T("Meek output: %S"), buffer);

        delete buffer;
    }
}
bool Meek::DoPeriodicCheck()
{
    // Check if we've lost the  process
    // pretty much copy/paste from LocalProxy

    if (m_meekProcessInfo.hProcess != 0)
    {
        // The polipo process handle will be signalled when the process terminates
        DWORD result = WaitForSingleObject(m_meekProcessInfo.hProcess, 0);

        if (result == WAIT_TIMEOUT)
        {
            // All normal, make sure meek has started and log its otput
            if(m_meekLocalPort > 0) {
                LogOutput();
            }

            if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
            {
                throw Abort();
            }

            return true;
        }
        else if (result == WAIT_OBJECT_0)
        {
            // The process has signalled -- which implies that it's died
            return false;
        }
        else
        {
            std::stringstream s;
            s << __FUNCTION__ << ": WaitForSingleObject failed (" << result << ", " << GetLastError() << ")";
            throw Error(s.str().c_str());
        }
    }
    return false;
}

void Meek::Cleanup()
{
     // Give the process an opportunity for graceful shutdown, then terminate
    if (m_meekProcessInfo.hProcess != 0
        && m_meekProcessInfo.hProcess != INVALID_HANDLE_VALUE)
    {
        StopProcess(m_meekProcessInfo.dwProcessId, m_meekProcessInfo.hProcess);
    }

    if (m_meekProcessInfo.hProcess != 0
        && m_meekProcessInfo.hProcess != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_meekProcessInfo.hProcess);
    }
    ZeroMemory(&m_meekProcessInfo, sizeof(m_meekProcessInfo));

    if (m_meekPipe!= 0
        && m_meekPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_meekPipe);
    }
    m_meekPipe = NULL;
}

bool Meek::StartMeek()
{
    tstringstream meekCommandLine;

    meekCommandLine << m_meekPath;

    STARTUPINFO meekStartupInfo;
    ZeroMemory(&meekStartupInfo, sizeof(meekStartupInfo));
    meekStartupInfo.cb = sizeof(meekStartupInfo);

    meekStartupInfo.dwFlags = STARTF_USESTDHANDLES;
    meekStartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateMeekPipe(meekStartupInfo.hStdOutput, meekStartupInfo.hStdError))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - CreateMeekPipe failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    if (!CreateProcess(
        m_meekPath.c_str(),
        (TCHAR*)meekCommandLine.str().c_str(),
            NULL,
            NULL,
            TRUE, // bInheritHandles
#ifdef _DEBUG
            CREATE_NEW_PROCESS_GROUP,
#else
            CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW,
#endif
            NULL,
            NULL,
            &meekStartupInfo,
            &m_meekProcessInfo))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - Meek CreateProcess failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    // Close the unneccesary handles
    CloseHandle(m_meekProcessInfo.hThread);
    m_meekProcessInfo.hThread = NULL;

    // Close child pipe handle (do not continue to modify the parent).
    // You need to make sure that no handles to the write end of the
    // output pipe are maintained in this process or else the pipe will
    // not close when the child process exits and the ReadFile will hang.
    
    if (!CloseHandle(meekStartupInfo.hStdOutput))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
    }
    if (!CloseHandle(meekStartupInfo.hStdError))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
    }
    
    WaitForInputIdle(m_meekProcessInfo.hProcess, 5000);

    return true;
}

bool Meek::CreateMeekPipe(HANDLE& o_outputPipe, HANDLE& o_errorPipe)
{
    m_meekPipe = INVALID_HANDLE_VALUE;
    o_outputPipe = INVALID_HANDLE_VALUE;
    o_errorPipe = INVALID_HANDLE_VALUE;

    HANDLE parentInputPipe = INVALID_HANDLE_VALUE, childStdinPipe = INVALID_HANDLE_VALUE;

    if (!CreateSubprocessPipes(
            m_meekPipe,
            parentInputPipe,
            childStdinPipe,
            o_outputPipe,
            o_errorPipe))
    {
        return false;
    }

    CloseHandle(parentInputPipe);
    CloseHandle(childStdinPipe);

    return true;
}

bool Meek::WaitForCmethodLine()
{
    assert(m_meekPipe != INVALID_HANDLE_VALUE);

    SetLastError(ERROR_SUCCESS);

    DWORD startTime = GetTickCount();
    DWORD bytes_avail = 0;
    bool isCmethod = false; 

    while(!isCmethod &&
        GetTickCountDiff(startTime, GetTickCount()) < MEEK_CONNECTION_TIMEOUT_SECONDS*1000)
    {


        if (!PeekNamedPipe(m_meekPipe, NULL, 0, NULL, &bytes_avail, NULL))
        {
            my_print(NOT_SENSITIVE, false, _T("%s:%d - PeekNamedPipe failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
            return false;
        }

        if (bytes_avail > 0)
        {
            char* buffer = new char[bytes_avail+1];
            DWORD num_read = 0;
            if (!ReadFile(m_meekPipe, buffer, bytes_avail, &num_read, NULL))
            {
                my_print(NOT_SENSITIVE, false, _T("%s:%d: ReadFile failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
                false;
            }
            buffer[bytes_avail] = '\0';

            // Note that we are only capturing Plonk output during the connect sequence.
            my_print(NOT_SENSITIVE, true, _T("%s:%d: Meek output: >>>%S<<<"), __TFUNCTION__, __LINE__, buffer);

            bool bGotListenPort = false;

            isCmethod = (strstr(buffer, "CMETHOD meek") != NULL);

            bool isCmethodsDone(strstr(buffer, "CMETHODS DONE") != NULL); 

            if(isCmethod)
            {
                bGotListenPort = ParseCmethodForPort(buffer);
            }

            delete[] buffer;

            
            //Do not wait any longer if CMETHODS DONE has 
            //been seen even if meek CMETHOD is absent
            if (isCmethod || isCmethodsDone)
            {
                return bGotListenPort;
            } 
        }

        Sleep(100);
    }
    //Output timed out
    return false;
}

bool Meek::ParseCmethodForPort(const char* str)
{
    const char* LOCALHOST_PREFIX = "127.0.0.1:";

    const char* localhost_prefix = strstr(str, "127.0.0.1:");
    if(localhost_prefix == NULL)
    {
        return false;
    }

    const char* port_start = localhost_prefix + strlen(LOCALHOST_PREFIX);

    int port = (int)strtol(string(port_start).c_str(), NULL, 10);
    if(port <= 0 || port > 65536) //check if port is valid
    {
        return false;
    }
    m_meekLocalPort = port;
    my_print(NOT_SENSITIVE, false, _T("Meek client is running on localhost port %d."), port);
    return true;
}

int Meek::GetListenPort()
{
    return m_meekLocalPort;
}
