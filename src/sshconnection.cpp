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

#include "stdafx.h"
#include "sshconnection.h"
#include "psiclient.h"
#include "config.h"

SSHConnection::SSHConnection(const bool& cancel)
: m_cancel(cancel)
{
    ZeroMemory(&m_plinkProcessInfo, sizeof(m_plinkProcessInfo));
    ZeroMemory(&m_polipoProcessInfo, sizeof(m_polipoProcessInfo));
}

SSHConnection::~SSHConnection(void)
{
    Disconnect();
}

extern HINSTANCE hInst;

bool ExtractExecutable(DWORD resourceID, tstring& path)
{
    // Extract executable from resources and write to temporary file

    HRSRC res;
    HGLOBAL handle = NULL;
    BYTE* data;
    DWORD size;

    res = FindResource(hInst, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    if (!res)
    {
        my_print(false, _T("ExtractExecutable - FindResource failed (%d)"), GetLastError());
        return false;
    }

    handle = LoadResource(NULL, res);
    if (!handle)
    {
        my_print(false, _T("ExtractExecutable - LoadResource failed (%d)"), GetLastError());
        return false;
    }

    data = (BYTE*)LockResource(handle);
    size = SizeofResource(NULL, res);

    DWORD ret;
    TCHAR tempPath[MAX_PATH];
    // http://msdn.microsoft.com/en-us/library/aa364991%28v=vs.85%29.aspx notes
    // tempPath can contain no more than MAX_PATH-14 characters
    ret = GetTempPath(MAX_PATH, tempPath);
    if (ret > MAX_PATH-14 || ret == 0)
    {
        my_print(false, _T("ExtractExecutable - GetTempPath failed (%d)"), GetLastError());
        return false;
    }

    TCHAR tempFileName[MAX_PATH];
    ret = GetTempFileName(tempPath, _T(""), 0, tempFileName);
    if (ret == 0)
    {
        my_print(false, _T("ExtractExecutable - GetTempFileName failed (%d)"), GetLastError());
        return false;
    }

    HANDLE tempFile = CreateFile(tempFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (tempFile == INVALID_HANDLE_VALUE) 
    { 
        my_print(false, _T("ExtractExecutable - CreateFile failed (%d)"), GetLastError());
        return false;
    }

    DWORD written = 0;
    if (!WriteFile(tempFile, data, size, &written, NULL)
        || written != size
        || !FlushFileBuffers(tempFile))
    {
        CloseHandle(tempFile);
        my_print(false, _T("ExtractExecutable - WriteFile/FlushFileBuffers failed (%d)"), GetLastError());
        return false;
    }

    CloseHandle(tempFile);

    path = tempFileName;

    return true;
}

bool SSHConnection::Connect(
        const tstring& sshServerAddress,
        const tstring& sshServerPort,
        const tstring& sshServerPublicKey,
        const tstring& sshUsername,
        const tstring& sshPassword)
{
    // Extract executables and put to disk if not already

    if (m_plinkPath.size() == 0)
    {
        if (!ExtractExecutable(IDR_PLINK_EXE, m_plinkPath))
        {
            return false;
        }
    }

    if (m_polipoPath.size() == 0)
    {
        if (!ExtractExecutable(IDR_POLIPO_EXE, m_polipoPath))
        {
            return false;
        }
    }

    // Ensure we start from a disconnected state

    Disconnect();

    // *** TODO: plink registry entry for server public key ***

    // Start plink using Psiphon server SSH parameters

    tstring plinkCommandLine = m_plinkPath
                               + _T(" -ssh -C -N")
                               + _T(" -P ") + sshServerPort
                               + _T(" -l ") + sshUsername
                               + _T(" -pw ") + sshPassword
                               + _T(" -D ") + PLINK_SOCKS_PROXY_PORT
                               + _T(" ") + sshServerAddress;

    STARTUPINFO plinkStartupInfo;
    ZeroMemory(&plinkStartupInfo, sizeof(plinkStartupInfo));
    plinkStartupInfo.cb = sizeof(plinkStartupInfo);

    if (!CreateProcess(
            m_plinkPath.c_str(),
            (TCHAR*)plinkCommandLine.c_str(),
            NULL,
            NULL,
            FALSE,
#ifdef _DEBUG
            CREATE_NEW_PROCESS_GROUP,
#else
            CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW,
#endif
            NULL,
            NULL,
            &plinkStartupInfo,
            &m_plinkProcessInfo))
    {
        my_print(false, _T("SSHConnection::Connect - Plink CreateProcess failed (%d)"), GetLastError());
        return false;
    }

    // TODO: wait for parent proxy to be in place? See comment in WaitForConnected for
    // various options; in testing, we found cases where Polipo stopped responding
    // when the ssh tunnel was torn down.

    // Start polipo, using plink's SOCKS proxy, with no disk cache and no web admin interface
    // (same recommended settings as Tor: http://www.pps.jussieu.fr/~jch/software/polipo/tor.html

    tstring polipoCommandLine = m_polipoPath
                                + _T(" proxyPort=") + POLIPO_HTTP_PROXY_PORT
                                + _T(" socksParentProxy=localhost:") + PLINK_SOCKS_PROXY_PORT
                                + _T(" diskCacheRoot=\"\"")
                                + _T(" disableLocalInterface=true");

    STARTUPINFO polipoStartupInfo;
    ZeroMemory(&polipoStartupInfo, sizeof(polipoStartupInfo));
    polipoStartupInfo.cb = sizeof(polipoStartupInfo);

    if (!CreateProcess(
            m_polipoPath.c_str(),
            (TCHAR*)polipoCommandLine.c_str(),
            NULL,
            NULL,
            FALSE,
#ifdef _DEBUG
            CREATE_NEW_PROCESS_GROUP,
#else
            CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW,
#endif
            NULL,
            NULL,
            &polipoStartupInfo,
            &m_polipoProcessInfo))
    {
        my_print(false, _T("SSHConnection::Connect - Polipo CreateProcess failed (%d)"), GetLastError());
        return false;
    }

    // *** TODO: configure Internet Options ***

    return true;
}

void SSHConnection::Disconnect(void)
{
    SignalDisconnect();
    WaitAndDisconnect();
}

bool SSHConnection::WaitForConnected(void)
{
    // TODO: This is just a tempoary solution. A couple of more robust approaches:
    // - Monitor stdout/stderr of plink/polipo for expected/unexpected messages
    // - OR poll HTTP requests to e.g., the Psiphon host's web server
    // - OR use ssh/http proxy libraries with APIs
    // - OR integrate plink/polipo source code

    // Simply wait 5 seconds to connect

    for (int i = 0; i < 50; i++)
    {
        if (m_cancel)
        {
            return false;
        }
        Sleep(100);
    }

    // Now that we are connected, change the Windows Internet Settings
    // to use our HTTP proxy

    m_systemProxySettings.Configure();

    return true;
}

void SSHConnection::WaitAndDisconnect(void)
{
    // See comment in WaitForConnected

    // Wait for either process to terminate, then clean up both
    // If the user cancels manually, m_cancel will be set -- we
    // handle that here while for VPN it's done in Manager

    while (m_plinkProcessInfo.hProcess != 0 && m_polipoProcessInfo.hProcess != 0)
    {
        HANDLE processes[2];
        processes[0] = m_plinkProcessInfo.hProcess;
        processes[1] = m_polipoProcessInfo.hProcess;

        DWORD result = WaitForMultipleObjects(2, processes, FALSE, 100);

        if (m_cancel || result != WAIT_TIMEOUT)
        {
            break;
        }
    }

    // Attempt graceful shutdown (for the case where one process
    // terminated unexpectedly, not a user cancel)
    SignalDisconnect();

    CloseHandle(m_plinkProcessInfo.hProcess);
    CloseHandle(m_plinkProcessInfo.hThread);
    ZeroMemory(&m_plinkProcessInfo, sizeof(m_plinkProcessInfo));

    CloseHandle(m_polipoProcessInfo.hProcess);
    CloseHandle(m_polipoProcessInfo.hThread);
    ZeroMemory(&m_polipoProcessInfo, sizeof(m_polipoProcessInfo));

    // Revert the Windows Internet Settings to the user's previous settings
    m_systemProxySettings.Revert();
}

void SSHConnection::SignalDisconnect(void)
{
    // Give each process an opportunity for graceful shutdown, then terminate

    if (m_plinkProcessInfo.hProcess != 0)
    {
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_plinkProcessInfo.dwProcessId);
        Sleep(100);
        TerminateProcess(m_plinkProcessInfo.hProcess, 0);
    }

    if (m_polipoProcessInfo.hProcess != 0)
    {
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_polipoProcessInfo.dwProcessId);
        Sleep(100);
        TerminateProcess(m_polipoProcessInfo.hProcess, 0);
    }
}
