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
:   m_cancel(cancel),
    m_polipoPipe(NULL),
    m_bytesTransferred(0),
    m_lastStatusSendTimeMS(0)
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
        const tstring& sshObfuscatedKey,
        const vector<std::regex>& statsRegexes)
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
    m_statsRegexes = statsRegexes;

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

        // Add host to Plonk's known host registry set
        // Note: currently we're not removing this after the session, so we're leaving a trace

        SetPlonkSSHHostKey(sshServerAddress, sshObfuscatedPort, sshServerHostKey);

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
        // Add host to Plonk's known host registry set

        SetPlonkSSHHostKey(sshServerAddress, sshServerPort, sshServerHostKey);

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

    // Close the unneccesary handles
    CloseHandle(m_plonkProcessInfo.hThread);
    m_plonkProcessInfo.hThread = NULL;

    WaitForInputIdle(m_plonkProcessInfo.hProcess, 5000);

    // TODO: wait for parent proxy to be in place? See comment in WaitForConnected for
    // various options; in testing, we found cases where Polipo stopped responding
    // when the ssh tunnel was torn down.

    // Start polipo, using plonk's SOCKS proxy, with no disk cache and no web admin interface
    // (same recommended settings as Tor: http://www.pps.jussieu.fr/~jch/software/polipo/tor.html

    tstring polipoCommandLine = m_polipoPath
                                + _T(" psiphonStats=true")
                                + _T(" proxyPort=") + POLIPO_HTTP_PROXY_PORT
                                + _T(" socksParentProxy=127.0.0.1:") + PLONK_SOCKS_PROXY_PORT
                                + _T(" diskCacheRoot=\"\"")
                                + _T(" disableLocalInterface=true")
                                + _T(" logLevel=1");

    STARTUPINFO polipoStartupInfo;
    ZeroMemory(&polipoStartupInfo, sizeof(polipoStartupInfo));
    polipoStartupInfo.cb = sizeof(polipoStartupInfo);

    polipoStartupInfo.dwFlags = STARTF_USESTDHANDLES;
    polipoStartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    polipoStartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    polipoStartupInfo.hStdOutput = CreatePolipoPipe();
    if (!polipoStartupInfo.hStdOutput)
    {
        my_print(false, _T("SSHConnection::Connect - CreatePolipoPipe failed (%d)"), GetLastError());
        return false;
    }

    if (!CreateProcess(
            m_polipoPath.c_str(),
            (TCHAR*)polipoCommandLine.c_str(),
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
            &polipoStartupInfo,
            &m_polipoProcessInfo))
    {
        my_print(false, _T("SSHConnection::Connect - Polipo CreateProcess failed (%d)"), GetLastError());
        return false;
    }

    // Close the unneccesary handles
    CloseHandle(m_polipoProcessInfo.hThread);
    m_polipoProcessInfo.hThread = NULL;

    // Close child pipe handle (do not continue to modify the parent).
    // You need to make sure that no handles to the write end of the
    // output pipe are maintained in this process or else the pipe will
    // not close when the child process exits and the ReadFile will hang.
    if (!CloseHandle(polipoStartupInfo.hStdOutput))
    {
        my_print(false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
    }

    WaitForInputIdle(m_polipoProcessInfo.hProcess, 5000);

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

    SOCKET sock = INVALID_SOCKET;
    WSAEVENT connectedEvent = WSACreateEvent();
    WSANETWORKEVENTS networkEvents;

    // Wait up to SSH_CONNECTION_TIMEOUT_SECONDS, checking periodically for user cancel

    DWORD start = GetTickCount();
    DWORD maxWaitMilliseconds = SSH_CONNECTION_TIMEOUT_SECONDS*1000;

    bool connected = false;

    while (true)
    {
        DWORD now = GetTickCount();

        if (now < start // Note: GetTickCount wraps after 49 days; small chance of a shorter timeout
            || now >= start + maxWaitMilliseconds)
        {
            break;
        }

        // Attempt to connect to SOCKS proxy
        // Just wait 100 ms. and then check for user cancel etc.

        closesocket(sock);
        sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (INVALID_SOCKET != sock
            && 0 == WSAEventSelect(sock, connectedEvent, FD_CONNECT)
            && SOCKET_ERROR == connect(sock, (SOCKADDR*)&plonkSocksServer, sizeof(plonkSocksServer))
            && WSAEWOULDBLOCK == WSAGetLastError()
            && WSA_WAIT_EVENT_0 == WSAWaitForMultipleEvents(1, &connectedEvent, TRUE, 100, FALSE)
            && 0 == WSAEnumNetworkEvents(sock, connectedEvent, &networkEvents)
            && (networkEvents.lNetworkEvents & FD_CONNECT)
            && networkEvents.iErrorCode[FD_CONNECT_BIT] == 0)
        {
            connected = true;
            break;
        }

        // If plonk aborted, give up

        if (WAIT_OBJECT_0 == WaitForSingleObject(m_plonkProcessInfo.hProcess, 0))
        {
            break;
        }

        // Check if user canceled

        if (m_cancel)
        {
            break;
        }
    }

    closesocket(sock);
    WSACloseEvent(connectedEvent);
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

    bool wasConnected = false;

    while (m_plonkProcessInfo.hProcess != 0 && m_polipoProcessInfo.hProcess != 0)
    {
        wasConnected = true;

        HANDLE wait_handles[] = { m_plonkProcessInfo.hProcess, 
                                  m_polipoProcessInfo.hProcess };

        DWORD result = WaitForMultipleObjects(sizeof(wait_handles)/sizeof(HANDLE), wait_handles, FALSE, 100); // 100 ms. = 1/10 second...

        if (m_cancel || result != WAIT_TIMEOUT)
        {
            break;
        }

        // We'll continue regardless of the stats-processing return status.
        (void)ProcessStatsAndStatus(connectionManager);
    }

    // Send a post-disconnect SSH session duration message

    if (wasConnected && connectionManager)
    {
        (void)ProcessStatsAndStatus(connectionManager, true);
    }

    // Attempt graceful shutdown (for the case where one process
    // terminated unexpectedly, not a user cancel)
    SignalDisconnect();

    CloseHandle(m_plonkProcessInfo.hProcess);
    ZeroMemory(&m_plonkProcessInfo, sizeof(m_plonkProcessInfo));

    CloseHandle(m_polipoProcessInfo.hProcess);
    ZeroMemory(&m_polipoProcessInfo, sizeof(m_polipoProcessInfo));

    CloseHandle(m_polipoPipe);
    m_polipoPipe = NULL;

    m_lastStatusSendTimeMS = 0;

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

// Create the pipe that will be used to communicate between the Polipo child 
// process and this process. The child write handle should be used as the stdin
// of the Polipo process.
// Returns NULL on error.
HANDLE SSHConnection::CreatePolipoPipe()
{
    // Most of this code is adapted from:
    // http://support.microsoft.com/kb/190351

    // Set up the security attributes struct.
    SECURITY_ATTRIBUTES sa;
    sa.nLength= sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    // Create the child output pipe.
    HANDLE hOutputReadTmp, hOutputWrite, hOutputRead;
    if (!CreatePipe(&hOutputReadTmp, &hOutputWrite, &sa, 0))
    {
        my_print(false, _T("%s:%d - CreatePipe failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return NULL;
    }

    // Create new output read handle and the input write handles. Set
    // the Properties to FALSE. Otherwise, the child inherits the
    // properties and, as a result, non-closeable handles to the pipes
    // are created.
    if (!DuplicateHandle(GetCurrentProcess(), hOutputReadTmp,
                         GetCurrentProcess(),
                         &hOutputRead, // Address of new handle.
                         0, 
                         FALSE, // Make it uninheritable.
                         DUPLICATE_SAME_ACCESS))
    {
        my_print(false, _T("%s:%d - DuplicateHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return NULL;
    }

    // Close inheritable copies of the handles you do not want to be
    // inherited.
    if (!CloseHandle(hOutputReadTmp))
    {
        my_print(false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return NULL;
    }

    m_polipoPipe = hOutputRead;

    return hOutputWrite;
}

// Check Polipo pipe for page view, bytes transferred, etc., info waiting to 
// be processed; gather info; process; send to server.
// Returns true on success, false otherwise.
bool SSHConnection::ProcessStatsAndStatus(
                        ConnectionManager* connectionManager, 
                        bool force/*=false*/)
{
    // Stats get sent to the server when a time or size limit has been reached.

    const unsigned int SEND_INTERVAL_MS = (5*60*1000); // 5 mins
    const unsigned int SEND_MAX_ENTRIES = 100;

    DWORD bytes_avail = 0;

    // On the very first call, m_lastStatusSendTimeMS will be 0, but we don't
    // want to send immediately. So...
    if (m_lastStatusSendTimeMS == 0) m_lastStatusSendTimeMS = GetTickCount();

    // ReadFile will block forever if there's no data to read, so we need
    // to check if there's data available to read first.
    if (!PeekNamedPipe(m_polipoPipe, NULL, 0, NULL, &bytes_avail, NULL))
    {
        my_print(false, _T("%s:%d - PeekNamedPipe failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
    }

    // If there's data available from the Polipo pipe, process it.
    if (bytes_avail > 0)
    {
        char* page_view_buffer = new char[bytes_avail+1];
        DWORD num_read = 0;
        if (!ReadFile(m_polipoPipe, page_view_buffer, bytes_avail, &num_read, NULL))
        {
            my_print(false, _T("%s:%d - ReadFile failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
            false;
        }
        page_view_buffer[bytes_avail] = '\0';

        // Update page view and traffic stats with the new info.
        ParsePolipoStatsBuffer(page_view_buffer);

        delete[] page_view_buffer;
    }

    // If the time or size thresholds have been exceeded, or if we're being 
    // forced to, send the stats.
    if (force
        || (m_lastStatusSendTimeMS + SEND_INTERVAL_MS) < GetTickCount()
        || m_pageViewEntries.size() >= SEND_MAX_ENTRIES)
    {
        connectionManager->SendStatusMessage(
            m_connectType, true, m_pageViewEntries, m_bytesTransferred);

        // Reset stats
        m_pageViewEntries.clear();
        m_bytesTransferred = 0;
        m_lastStatusSendTimeMS = GetTickCount();
    }

    return true;
}

/* Store page view info. Some transformation may be done depending on the 
   contents of m_statsRegexes. 
*/
void SSHConnection::UpsertPageView(const string& entry)
{
    if (entry.length() <= 0)
    {
        return;
    }

    string store_entry = "(OTHER)";

    bool match = false;
    for (unsigned int i = 0; i < m_statsRegexes.size(); i++)
    {
        if (regex_search(
                entry, 
                m_statsRegexes[i]))
        {
            store_entry = entry;
            break;
        }
    }

    // Add/increment the entry.
    map<string, int>::iterator map_entry = m_pageViewEntries.find(store_entry);
    if (map_entry == m_pageViewEntries.end())
    {
        m_pageViewEntries[store_entry] = 1;
    }
    else
    {
        map_entry->second += 1;
    }
}

void SSHConnection::ParsePolipoStatsBuffer(const char* page_view_buffer)
{
    const char* HTTP_PREFIX = "PSIPHON-PAGE-VIEW-HTTP:>>";
    const char* HTTPS_PREFIX = "PSIPHON-PAGE-VIEW-HTTPS:>>";
    const char* BYTES_TRANSFERRED_PREFIX = "PSIPHON-BYTES-TRANSFERRED:>>";
    const char* ENTRY_END = "<<";

    const char* curr_pos = page_view_buffer;
    const char* end_pos = page_view_buffer + strlen(page_view_buffer);

    while (curr_pos < end_pos)
    {
        const char* http_entry_start = strstr(curr_pos, HTTP_PREFIX);
        const char* https_entry_start = strstr(curr_pos, HTTPS_PREFIX);
        const char* bytes_transferred_start = strstr(curr_pos, BYTES_TRANSFERRED_PREFIX);
        const char* entry_end = NULL;

        if (http_entry_start == NULL) http_entry_start = end_pos;
        if (https_entry_start == NULL) https_entry_start = end_pos;
        if (bytes_transferred_start == NULL) bytes_transferred_start = end_pos;

        const char* next = min(http_entry_start, min(https_entry_start, bytes_transferred_start));
        if (next >= end_pos)
        {
            // No next entry found
            break;
        }

        // Find the next entry

        if (next == http_entry_start)
        {
            const char* entry_start = next + strlen(HTTP_PREFIX);
            entry_end = strstr(entry_start, ENTRY_END);
            
            if (!entry_end)
            {
                // Something is rather wrong. Maybe an incomplete entry.
                // Stop processing;
                break;
            }

            UpsertPageView(string(entry_start, entry_end-entry_start));
        }
        else if (next == https_entry_start)
        {
            const char* entry_start = next + strlen(HTTPS_PREFIX);
            entry_end = strstr(entry_start, ENTRY_END);

            if (!entry_end)
            {
                // Something is rather wrong. Maybe an incomplete entry.
                // Stop processing;
                break;
            }

            UpsertPageView(string(entry_start, entry_end-entry_start));
        }
        else if (next == bytes_transferred_start)
        {
            const char* entry_start = next + strlen(BYTES_TRANSFERRED_PREFIX);
            entry_end = strstr(entry_start, ENTRY_END);

            if (!entry_end)
            {
                // Something is rather wrong. Maybe an incomplete entry.
                // Stop processing;
                break;
            }

            m_bytesTransferred += atoi(string(entry_start, entry_end-entry_start).c_str());
        }
        else
        {
            // Shouldn't get here...
            // ASSERT(0);
            break;
        }

        curr_pos = entry_end + strlen(ENTRY_END);
    }
}

