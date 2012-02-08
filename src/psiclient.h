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


//==== logging =========================================================

void my_print(bool bDebugMessage, const TCHAR* format, ...);
void my_print(bool bDebugMessage, const string& message);

#ifdef UNICODE
    #define WIDEN2(x) L##x
    #define WIDEN(x) WIDEN2(x)
    #define __WFILE__ WIDEN(__FILE__)
    #define __WFUNCTION__ WIDEN(__FUNCTION__)
    #define __TFILE__ __WFILE__
    #define __TFUNCTION__ __WFUNCTION__
#else
    #define __TFILE__ __FILE__
    #define __TFILE__ __FUNCTION__
#endif


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
        if (m_logInfo.length()>0) my_print(true, _T("%s: obtaining 0x%x: %s"), __TFUNCTION__, (int)m_mutex, m_logInfo.c_str());
        WaitForSingleObject(m_mutex, INFINITE);
        if (m_logInfo.length()>0) my_print(true, _T("%s: obtained 0x%x: %s"), __TFUNCTION__, (int)m_mutex, m_logInfo.c_str());
    }

    ~AutoMUTEX() 
    {
        if (m_logInfo.length()>0) my_print(true, _T("%s: releasing 0x%x: %s"), __TFUNCTION__, (int)m_mutex, m_logInfo.c_str());
        ReleaseMutex(m_mutex);
    }
private:
    HANDLE m_mutex;
    tstring m_logInfo;
};

#define AUTOMUTEX(mutex) 
