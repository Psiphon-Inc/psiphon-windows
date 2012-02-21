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

#include <windows.h> 


// Copied from http://support.microsoft.com/kb/243953

class LimitSingleInstance
{
protected:
    DWORD  m_dwLastError;
    HANDLE m_hMutex;

public:
    LimitSingleInstance(TCHAR *strMutexName)
    {
        // Make sure that you use a name that is unique for this application otherwise
        // two apps may think they are the same if they are using same name for
        // 3rd parm to CreateMutex
        
        // The app may be restarting (self-upgrade) so try this a few times
        // while the mutex already exists.
        for (int i = 0; i < 4; ++i)
        {
            Sleep(i * 100);
            m_hMutex = CreateMutex(NULL, FALSE, strMutexName); // do early
            if (ERROR_ALREADY_EXISTS != (m_dwLastError = GetLastError()))
            {
                break;
            }
        }
    }

    ~LimitSingleInstance()
    {
        if (m_hMutex)  // Do not forget to close handles.
        {
            CloseHandle(m_hMutex); // Do as late as possible.
            m_hMutex = NULL; // Good habit to be in.
        }
    }

    BOOL IsAnotherInstanceRunning()
    {
        return (ERROR_ALREADY_EXISTS == m_dwLastError);
    }
};
