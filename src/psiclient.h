/*
 * Copyright (c) 2011, Psiphon Inc.
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

#include "resource.h"


//==== global constants ================================================

#define WM_PSIPHON_MY_PRINT            WM_USER + 100
#define WM_PSIPHON_FEEDBACK_SUCCESS    WM_USER + 101
#define WM_PSIPHON_FEEDBACK_FAILED     WM_USER + 102
#define WM_PSIPHON_CREATED             WM_USER + 103


//==== miscellaneous====================================================

bool GetSplitTunnel();


//==== logging =========================================================

enum LogSensitivity
{
    /**
     The log does not contain sensitive information.
     */
    NOT_SENSITIVE,
            
    /**
     The log message itself is sensitive information.
     */
    SENSITIVE_LOG,
            
    /**
     The format arguments to the log messages are sensitive, but the 
     log message itself is not. 
     */
    SENSITIVE_FORMAT_ARGS
};

void my_print(LogSensitivity sensitivity, bool bDebugMessage, const TCHAR* format, ...);
void my_print(LogSensitivity sensitivity, bool bDebugMessage, const string& message);


struct MessageHistoryEntry
{
    tstring message;
    tstring timestamp;
    bool debug;
};

void GetMessageHistory(vector<MessageHistoryEntry>& history);


//==== global helpers ==================================================

class AutoHANDLE
{
public:
    AutoHANDLE(HANDLE handle) {m_handle = handle;}
    ~AutoHANDLE() {CloseHandle(m_handle);}
    operator HANDLE() {return m_handle;}
private:
    HANDLE m_handle;
};


class AutoMUTEX
{
public:
    AutoMUTEX(HANDLE mutex, TCHAR* logInfo=0) : m_mutex(mutex)
    {
        if (logInfo) m_logInfo = logInfo;
        if (m_logInfo.length()>0) my_print(NOT_SENSITIVE, true, _T("%s: obtaining 0x%x: %s"), __TFUNCTION__, (int)m_mutex, m_logInfo.c_str());
        WaitForSingleObject(m_mutex, INFINITE);
        if (m_logInfo.length()>0) my_print(NOT_SENSITIVE, true, _T("%s: obtained 0x%x: %s"), __TFUNCTION__, (int)m_mutex, m_logInfo.c_str());
    }

    ~AutoMUTEX() 
    {
        if (m_logInfo.length()>0) my_print(NOT_SENSITIVE, true, _T("%s: releasing 0x%x: %s"), __TFUNCTION__, (int)m_mutex, m_logInfo.c_str());
        ReleaseMutex(m_mutex);
    }
private:
    HANDLE m_mutex;
    tstring m_logInfo;
};

#define AUTOMUTEX(mutex) 


//==== miscellaneous====================================================

bool GetSplitTunnel();
tstring GetSelectedTransport();
