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
#include <WinSock2.h>
#include <WinCrypt.h>
#include "sshconnection.h"
#include "connectionmanager.h"
#include "httpsrequest.h"
#include "psiclient.h"
#include "config.h"

SSHConnection::SSHConnection(const bool& cancel)
:   m_cancel(cancel)
{
    ZeroMemory(&m_plonkProcessInfo, sizeof(m_plonkProcessInfo));
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
    HGLOBAL handle = INVALID_HANDLE_VALUE;
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

bool SetPlonkSSHHostKey(
        const tstring& sshServerAddress,
        const tstring& sshServerPort,
        const tstring& sshServerHostKey)
{
    // Add Plonk registry entry for host for non-interactive host key validation

    // Host key is base64 encoded set of fiels

    BYTE* decodedFields = NULL;
    DWORD size = 0;

    if (!CryptStringToBinary(sshServerHostKey.c_str(), sshServerHostKey.length(), CRYPT_STRING_BASE64, NULL, &size, NULL, NULL)
        || !(decodedFields = new (std::nothrow) BYTE[size])
        || !CryptStringToBinary(sshServerHostKey.c_str(), sshServerHostKey.length(), CRYPT_STRING_BASE64, decodedFields, &size, NULL, NULL))
    {
        my_print(false, _T("SetPlonkSSHHostKey: CryptStringToBinary failed (%d)"), GetLastError());
        return false;
    }

    // field format: {<4 byte len (big endian), len bytes field>}+
    // first field is key type, expecting "ssh-rsa";
    // remaining fields are opaque number value -- simply emit in new format which is comma delimited hex strings

    const char* expectedKeyTypeValue = "ssh-rsa";
    unsigned long expectedKeyTypeLen = htonl(strlen(expectedKeyTypeValue));

    if (memcmp(decodedFields + 0, &expectedKeyTypeLen, sizeof(unsigned long))
        || memcmp(decodedFields + sizeof(unsigned long), expectedKeyTypeValue, strlen(expectedKeyTypeValue)))
    {
        delete [] decodedFields;

        my_print(false, _T("SetPlonkSSHHostKey: unexpected key type"));
        return false;
    }

    string data;

    unsigned long offset = sizeof(unsigned long) + strlen(expectedKeyTypeValue);

    while (offset < size - sizeof(unsigned long))
    {
        unsigned long nextLen = ntohl(*((long*)(decodedFields + offset)));
        offset += sizeof(unsigned long);

        if (nextLen > 0 && offset + nextLen <= size)        
        {
            string field = "";
            const char* hexDigits = "0123456789abcdef";
            for (unsigned long i = 0; i < nextLen; i++)
            {
                char digit = hexDigits[decodedFields[offset + i] >> 4];
                // Don't add leading zeroes
                if (digit != '0' || field.length() > 0) field += digit;
                digit = hexDigits[decodedFields[offset + i] & 0x0F];
                // Always include last nibble (e.g. 0x0)
                if (i == nextLen-1 || (digit != '0' || field.length() > 0)) field += digit;
            }
            field = "0x" + field;
            if (data.length() > 0) data += ",";
            data += field;
            offset += nextLen;
        }
    }

    delete [] decodedFields;

    string value = string("rsa2@") + TStringToNarrow(sshServerPort) + ":" + TStringToNarrow(sshServerAddress);

    const TCHAR* plonkRegistryKey = _T("Software\\SimonTatham\\PuTTY\\SshHostKeys");

    HKEY key = 0;
    LONG returnCode = RegCreateKeyEx(HKEY_CURRENT_USER, plonkRegistryKey, 0, 0, 0, KEY_WRITE, 0, &key, NULL);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("SetPlonkSSHHostKey: Create Registry Key failed (%d)"), returnCode);
        return false;
    }

    returnCode = RegSetValueExA(key, value.c_str(), 0, REG_SZ, (PBYTE)data.c_str(), data.length()+1);
    if (ERROR_SUCCESS != returnCode)
    {
        RegCloseKey(key);

        my_print(false, _T("SetPlonkSSHHostKey: Set Registry Value failed (%d)"), returnCode);
        return false;
    }

    RegCloseKey(key);

    return true;
}

bool SSHConnection::Connect(
        int connectType,
        const tstring& sshServerAddress,
        const tstring& sshServerPort,
        const tstring& sshServerHostKey,
        const tstring& sshUsername,
        const tstring& sshPassword,
        const tstring& sshObfuscatedPort,
        const tstring& sshObfuscatedKey)
{
    my_print(false, _T("SSH connecting..."));

    // Extract executables and put to disk if not already

    if (m_plonkPath.size() == 0)
    {
        if (!ExtractExecutable(IDR_PLONK_EXE, m_plonkPath))
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

    // Ensure we start from a disconnected/clean state

    Disconnect();

    m_connectType = connectType;

    // Add host to Plonk's known host registry set
    // Note: currently we're not removing this after the session, so we're leaving a trace

    SetPlonkSSHHostKey(sshServerAddress, sshServerPort, sshServerHostKey);

    // Start plonk using Psiphon server SSH parameters

    // Note: -batch ensures plonk doesn't hang on a prompt when the server's host key isn't
    // the expected value we just set in the registry

    tstring plonkCommandLine;
    
    if (connectType == SSH_CONNECT_OBFUSCATED)
    {
        if (sshObfuscatedPort.size() <= 0 || sshObfuscatedKey.size() <= 0)
        {
            my_print(false, _T("SSHConnection::Connect - missing parameters"));
            return false;
        }

        plonkCommandLine = m_plonkPath
                                + _T(" -ssh -C -N -batch")
                                + _T(" -P ") + sshObfuscatedPort
                                + _T(" -z -Z ") + sshObfuscatedKey
                                + _T(" -l ") + sshUsername
                                + _T(" -pw ") + sshPassword
                                + _T(" -D ") + PLONK_SOCKS_PROXY_PORT
                                + _T(" ") + sshServerAddress;
    }
    else
    {
        plonkCommandLine = m_plonkPath
                                + _T(" -ssh -C -N -batch")
                                + _T(" -P ") + sshServerPort
                                + _T(" -l ") + sshUsername
                                + _T(" -pw ") + sshPassword
                                + _T(" -D ") + PLONK_SOCKS_PROXY_PORT
                                + _T(" ") + sshServerAddress;
    }

    STARTUPINFO plonkStartupInfo;
    ZeroMemory(&plonkStartupInfo, sizeof(plonkStartupInfo));
    plonkStartupInfo.cb = sizeof(plonkStartupInfo);

    if (!CreateProcess(
            m_plonkPath.c_str(),
            (TCHAR*)plonkCommandLine.c_str(),
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
            &plonkStartupInfo,
            &m_plonkProcessInfo))
    {
        my_print(false, _T("SSHConnection::Connect - Plonk CreateProcess failed (%d)"), GetLastError());
        return false;
    }

    // TODO: wait for parent proxy to be in place? See comment in WaitForConnected for
    // various options; in testing, we found cases where Polipo stopped responding
    // when the ssh tunnel was torn down.

    // Start polipo, using plonk's SOCKS proxy, with no disk cache and no web admin interface
    // (same recommended settings as Tor: http://www.pps.jussieu.fr/~jch/software/polipo/tor.html

    tstring polipoCommandLine = m_polipoPath
                                + _T(" proxyPort=") + POLIPO_HTTP_PROXY_PORT
                                + _T(" socksParentProxy=127.0.0.1:") + PLONK_SOCKS_PROXY_PORT
                                + _T(" diskCacheRoot=\"\"")
                                + _T(" disableLocalInterface=true")
                                + _T(" logLevel=1");

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

    return true;
}

void SSHConnection::Disconnect(void)
{
    SignalDisconnect();
    WaitAndDisconnect(0);
    m_connectType = -1;
}

bool SSHConnection::WaitForConnected(void)
{
    // There are a number of options for monitoring the connected status
    // of plonk/polipo. We're going with a quick and dirty solution of
    // (a) monitoring the child processes -- if they exit, there was an error;
    // (b) asynchronously connecting to the plonk SOCKS server, which isn't
    //     started by plonk until its ssh tunnel is established.
    // Note: piping stdout/stderr of the child processes and monitoring
    // messages is problematic because we don't control the C I/O flushing
    // of these processes (http://support.microsoft.com/kb/190351).
    // Additional measures or alternatives include making actual HTTP
    // requests through the entire stack from time to time or switching
    // to integrated ssh/http libraries with APIs.

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    sockaddr_in plonkSocksServer;
    plonkSocksServer.sin_family = AF_INET;
    plonkSocksServer.sin_addr.s_addr = inet_addr("127.0.0.1");
    plonkSocksServer.sin_port = htons(atoi(TStringToNarrow(PLONK_SOCKS_PROXY_PORT).c_str()));

    SOCKET sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    WSAEVENT connectedEvent = WSACreateEvent();

    bool connected = false;

    if (0 == WSAEventSelect(sock, connectedEvent, FD_CONNECT)
        && SOCKET_ERROR == connect(sock, (SOCKADDR*)&plonkSocksServer, sizeof(plonkSocksServer))
        && WSAEWOULDBLOCK == WSAGetLastError())
    {
        // Wait up to SSH_CONNECTION_TIMEOUT_SECONDS, checking periodically for user cancel

        for (int i = 0; i < SSH_CONNECTION_TIMEOUT_SECONDS*10; i++)
        {
            if (WSA_WAIT_EVENT_0 == WSAWaitForMultipleEvents(1, &connectedEvent, TRUE, 100, FALSE))
            {
                connected = true;
                break;
            }
            else if (m_cancel)
            {
                break;
            }
        }
    }

    closesocket(sock);
    WSACleanup();

    if (connected)
    {
        // Now that we are connected, change the Windows Internet Settings
        // to use our HTTP proxy

        m_systemProxySettings.Configure();

        my_print(false, _T("SSH successfully connected."));
    }

    return connected;
}

void SSHConnection::WaitAndDisconnect(ConnectionManager* connectionManager)
{
    // Regarding process monitoring: see comment in WaitForConnected

    // Wait for either process to terminate, then clean up both
    // If the user cancels manually, m_cancel will be set -- we
    // handle that here while for VPN it's done in Manager

    unsigned int totalTenthsSeconds = 0;

    bool wasConnected = false;

    while (m_plonkProcessInfo.hProcess != 0 && m_polipoProcessInfo.hProcess != 0)
    {
        wasConnected = true;

        HANDLE processes[2];
        processes[0] = m_plonkProcessInfo.hProcess;
        processes[1] = m_polipoProcessInfo.hProcess;

        DWORD result = WaitForMultipleObjects(2, processes, FALSE, 100); // 100 ms. = 1/10 second...

        if (m_cancel || result != WAIT_TIMEOUT)
        {
            break;
        }

        // Very basic client-side SSH session duration stats are implemented by sending
        // status messages at the time buckets we're interested in. The intermediate
        // updates allow a session duration to be estimated in the case where a post
        // disconnect update doesn't arrive.
        // 15 sec, 1 min, 5 min, 30 min, 1 hour, every hour, end of session

        totalTenthsSeconds++; // wraps after 4971 days...

        if (totalTenthsSeconds == 150
            || totalTenthsSeconds == 600
            || totalTenthsSeconds == 3000
            || totalTenthsSeconds == 18000
            || 0 == (totalTenthsSeconds % 36000))
        {
            if (connectionManager)
            {
                connectionManager->SendStatusMessage(m_connectType, true);
            }
        }
    }

    // Send a post-disconnect SSH session duration message

    if (wasConnected && connectionManager)
    {
        connectionManager->SendStatusMessage(m_connectType, false);
    }

    // Attempt graceful shutdown (for the case where one process
    // terminated unexpectedly, not a user cancel)
    SignalDisconnect();

    CloseHandle(m_plonkProcessInfo.hProcess);
    CloseHandle(m_plonkProcessInfo.hThread);
    ZeroMemory(&m_plonkProcessInfo, sizeof(m_plonkProcessInfo));

    CloseHandle(m_polipoProcessInfo.hProcess);
    CloseHandle(m_polipoProcessInfo.hThread);
    ZeroMemory(&m_polipoProcessInfo, sizeof(m_polipoProcessInfo));

    // Revert the Windows Internet Settings to the user's previous settings
    m_systemProxySettings.Revert();

    if (wasConnected)
    {
        my_print(false, _T("SSH disconnected."));
    }
}

void SSHConnection::SignalDisconnect(void)
{
    // Give each process an opportunity for graceful shutdown, then terminate

    if (m_plonkProcessInfo.hProcess != 0)
    {
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_plonkProcessInfo.dwProcessId);
        Sleep(100);
        TerminateProcess(m_plonkProcessInfo.hProcess, 0);
    }

    if (m_polipoProcessInfo.hProcess != 0)
    {
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_polipoProcessInfo.dwProcessId);
        Sleep(100);
        TerminateProcess(m_polipoProcessInfo.hProcess, 0);
    }
}
