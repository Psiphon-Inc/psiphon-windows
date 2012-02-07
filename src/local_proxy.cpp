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

#include "stdafx.h"
#include "local_proxy.h"
#include "psiclient.h"
#include "utilities.h"
#include "sessioninfo.h"
#include "systemproxysettings.h"
#include <Shlwapi.h>



#define POLIPO_CONNECTION_TIMEOUT_SECONDS   20
#define POLIPO_HTTP_PROXY_PORT              8080
#define POLIPO_EXE_NAME                     _T("psiphon3-polipo.exe")


LocalProxy::LocalProxy(
                ILocalProxyStatsCollector* statsCollector, 
                const SessionInfo& sessionInfo, 
                SystemProxySettings* systemProxySettings,
                int parentPort, 
                const tstring& splitTunnelingFilePath)
    : m_statsCollector(statsCollector),
      m_systemProxySettings(systemProxySettings),
      m_parentPort(parentPort),
      m_polipoPipe(NULL),
      m_bytesTransferred(0),
      m_lastStatusSendTimeMS(0),
      m_splitTunnelingFilePath(splitTunnelingFilePath)
{
    ZeroMemory(&m_polipoProcessInfo, sizeof(m_polipoProcessInfo));

    m_pageViewRegexes = sessionInfo.GetPageViewRegexes();
    m_httpsRequestRegexes = sessionInfo.GetHttpsRequestRegexes();

    assert(statsCollector);
    assert(systemProxySettings);
}

LocalProxy::~LocalProxy()
{
    IWorkerThread::Stop();

    Cleanup();
}


bool LocalProxy::DoStart()
{
    if (m_polipoPath.size() == 0)
    {
        if (!ExtractExecutable(IDR_POLIPO_EXE, POLIPO_EXE_NAME, m_polipoPath))
        {
            return false;
        }
    }

    // Ensure we start from a disconnected/clean state
    Cleanup();

    if (!StartPolipo())
    {
        Cleanup();
        return false;
    }

    // Now that we are connected, change the Windows Internet Settings
    // to use our HTTP proxy (not actually applied until later).

    m_systemProxySettings->SetHttpProxyPort(POLIPO_HTTP_PROXY_PORT);
    m_systemProxySettings->SetHttpsProxyPort(POLIPO_HTTP_PROXY_PORT);

    my_print(true, _T("Polipo successfully started."));

    return true;
}

bool LocalProxy::DoPeriodicCheck()
{
    // Check if we've lost the Polipo process

    if (m_polipoProcessInfo.hProcess != 0)
    {
        // The polipo process handle will be signalled when the process terminates
        DWORD result = WaitForSingleObject(m_polipoProcessInfo.hProcess, 0);

        if (result == WAIT_TIMEOUT)
        {
            // Everything normal; process stats and return

            // We don't care about the return value of ProcessStatsAndStatus
            (void)ProcessStatsAndStatus(true);

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

    // If we're here, then there's no Polipo process at all (which is weird, but...)
    return false;
}

void LocalProxy::DoStop()
{
    if (m_polipoProcessInfo.hProcess != 0)
    {
        // We were (probably) connected, so send a final stats message
        (void)ProcessStatsAndStatus(false);
        my_print(false, _T("Local proxy closed."));
    }
}

void LocalProxy::Cleanup()
{
    // Give the process an opportunity for graceful shutdown, then terminate
    if (m_polipoProcessInfo.hProcess != 0
        && m_polipoProcessInfo.hProcess != INVALID_HANDLE_VALUE)
    {
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_polipoProcessInfo.dwProcessId);
        WaitForSingleObject(m_polipoProcessInfo.hProcess, 100);
        TerminateProcess(m_polipoProcessInfo.hProcess, 0);
    }

    if (m_polipoProcessInfo.hProcess != 0
        && m_polipoProcessInfo.hProcess != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_polipoProcessInfo.hProcess);
    }
    ZeroMemory(&m_polipoProcessInfo, sizeof(m_polipoProcessInfo));

    if (m_polipoPipe!= 0
        && m_polipoPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_polipoPipe);
    }
    m_polipoPipe = NULL;

    m_lastStatusSendTimeMS = 0;
}


bool LocalProxy::StartPolipo()
{
    // Start polipo, with no disk cache and no web admin interface
    // (same recommended settings as Tor: http://www.pps.jussieu.fr/~jch/software/polipo/tor.html

    tstringstream polipoCommandLine;

    polipoCommandLine << m_polipoPath
                      << _T(" psiphonStats=true")
                      << _T(" proxyPort=") << POLIPO_HTTP_PROXY_PORT
                      << _T(" diskCacheRoot=\"\"")
                      << _T(" disableLocalInterface=true")
                      << _T(" logLevel=1");

    // Use the parent proxy, if one is available for the current transport
    // also do split tunneling if there is a parent proxy 
    if (m_parentPort > 0)
    {
        polipoCommandLine << _T(" socksParentProxy=127.0.0.1:") << m_parentPort;
        if(m_splitTunnelingFilePath.length() > 0)
        {
            polipoCommandLine << _T(" splitTunnelingFile=\"") << m_splitTunnelingFilePath << _T("\"");

            //TODO: the DNS for split tunneling is hardcoded. Make it a part of handshake or 
            //create another tunnel for DNS in the future?
            polipoCommandLine << _T(" splitTunnelingDnsServer=8.8.8.8");
        }
    }

    STARTUPINFO polipoStartupInfo;
    ZeroMemory(&polipoStartupInfo, sizeof(polipoStartupInfo));
    polipoStartupInfo.cb = sizeof(polipoStartupInfo);

    polipoStartupInfo.dwFlags = STARTF_USESTDHANDLES;
    polipoStartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreatePolipoPipe(polipoStartupInfo.hStdOutput, polipoStartupInfo.hStdError))
    {
        my_print(false, _T("%s - CreatePolipoPipe failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    if (!CreateProcess(
            m_polipoPath.c_str(),
            (TCHAR*)polipoCommandLine.str().c_str(),
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
        my_print(false, _T("%s - Polipo CreateProcess failed (%d)"), __TFUNCTION__, GetLastError());
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

    DWORD connected = WaitForConnectability(
                        POLIPO_HTTP_PROXY_PORT,
                        POLIPO_CONNECTION_TIMEOUT_SECONDS*1000,
                        m_polipoProcessInfo.hProcess,
                        GetSignalStopEvent());

    if (ERROR_OPERATION_ABORTED == connected)
    {
        throw Abort();
    }
    else if (ERROR_SUCCESS != connected)
    {
        my_print(false, _T("%s - Failed to connect to Polipo (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    return true;
}


// Create the pipe that will be used to communicate between the Polipo child 
// process and this process. o_outputPipe write handle should be used as the stdout
// of the Polipo process, and o_errorPipe as stderr. 
// (Both pipes are really the same, but duplicated.)
// Returns true on success.
bool LocalProxy::CreatePolipoPipe(HANDLE& o_outputPipe, HANDLE& o_errorPipe)
{
    o_outputPipe = INVALID_HANDLE_VALUE;
    o_errorPipe = INVALID_HANDLE_VALUE;

    // Most of this code is adapted from:
    // http://support.microsoft.com/kb/190351

    // Set up the security attributes struct.
    SECURITY_ATTRIBUTES sa;
    sa.nLength= sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hOutputReadTmp, hOutputWrite, hOutputRead, hErrorWrite;

    // Create the child output pipe.
    if (!CreatePipe(&hOutputReadTmp, &hOutputWrite, &sa, 0))
    {
        my_print(false, _T("%s:%d - CreatePipe failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
    }

    // Create a duplicate of the output write handle for the std error
    // write handle. This is necessary in case the child application
    // closes one of its std output handles.
    if (!DuplicateHandle(
            GetCurrentProcess(), hOutputWrite, 
            GetCurrentProcess(), &hErrorWrite,
            0, TRUE, DUPLICATE_SAME_ACCESS))
    {
        my_print(false, _T("%s:%d - DuplicateHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
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
        return false;
    }

    // Close inheritable copies of the handles you do not want to be
    // inherited.
    if (!CloseHandle(hOutputReadTmp))
    {
        my_print(false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
    }

    m_polipoPipe = hOutputRead;
    o_outputPipe = hOutputWrite;
    o_errorPipe = hErrorWrite;

    return true;
}

// Check Polipo pipe for page view, bytes transferred, etc., info waiting to 
// be processed; gather info; process; send to server.
// If connected is true, the stats will only be sent to the server if certain
// time or size limits have been exceeded; if connected is false, the stats will
// be sent regardlesss of limits.
// Returns true on success, false otherwise.
bool LocalProxy::ProcessStatsAndStatus(bool connected)
{
    // Stats get sent to the server when a time or size limit has been reached.

    const DWORD DEFAULT_SEND_INTERVAL_MS = (30*60*1000); // 30 mins
    const unsigned int DEFAULT_SEND_MAX_ENTRIES = 1000;  // This is mostly to bound memory usage
    static DWORD s_send_interval_ms = DEFAULT_SEND_INTERVAL_MS;
    static unsigned int s_send_max_entries = DEFAULT_SEND_MAX_ENTRIES;

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

    // Note: GetTickCount wraps after 49 days; small chance of a shorter timeout
    DWORD now = GetTickCount();
    if (now < m_lastStatusSendTimeMS) m_lastStatusSendTimeMS = 0;

    // If the time or size thresholds have been exceeded, or if we're being 
    // forced to, send the stats.
    if (!connected
        || (m_lastStatusSendTimeMS + s_send_interval_ms) < now
        || m_pageViewEntries.size() >= s_send_max_entries
        || m_httpsRequestEntries.size() >= s_send_max_entries)
    {
        if (m_statsCollector->SendStatusMessage(
                                connected, // Note: there's a timeout side-effect when connected=false
                                m_pageViewEntries, 
                                m_httpsRequestEntries, 
                                m_bytesTransferred))
        {
            // Reset thresholds
            s_send_interval_ms = DEFAULT_SEND_INTERVAL_MS;
            s_send_max_entries = DEFAULT_SEND_MAX_ENTRIES;

            // Reset stats
            m_pageViewEntries.clear();
            m_httpsRequestEntries.clear();
            m_bytesTransferred = 0;
            m_lastStatusSendTimeMS = now;
        }
        else
        {
            // Status sending failures are fairly common.
            // We'll back off the thresholds and try again later.
            s_send_interval_ms += DEFAULT_SEND_INTERVAL_MS;
            s_send_max_entries += DEFAULT_SEND_MAX_ENTRIES;
        }
    }

    return true;
}

/* Store page view info. Some transformation may be done depending on the 
   contents of m_pageViewRegexes. 
*/
void LocalProxy::UpsertPageView(const string& entry)
{
    if (entry.length() <= 0) return;

    my_print(true, _T("%s:%d: %S"), __TFUNCTION__, __LINE__, entry.c_str());

    string store_entry = "(OTHER)";

    for (unsigned int i = 0; i < m_pageViewRegexes.size(); i++)
    {
        if (regex_match(entry, m_pageViewRegexes[i].regex))
        {
            store_entry = regex_replace(
                            entry, 
                            m_pageViewRegexes[i].regex, 
                            m_pageViewRegexes[i].replace);
            break;
        }
    }

    if (store_entry.length() == 0) return;

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

/* Store HTTPS request info. Some transformation may be done depending on the 
   contents of m_httpsRequestRegexes. 
*/
void LocalProxy::UpsertHttpsRequest(string entry)
{
    // First of all, get rid of any trailing port number.
    string::size_type port = entry.find_last_of(':');
    if (port != entry.npos)
    {
        entry.erase(entry.begin()+port, entry.end());
    }

    if (entry.length() <= 0) return;

    string store_entry = "(OTHER)";

    for (unsigned int i = 0; i < m_httpsRequestRegexes.size(); i++)
    {
        if (regex_match(entry, m_httpsRequestRegexes[i].regex))
        {
            store_entry = regex_replace(
                            entry, 
                            m_httpsRequestRegexes[i].regex, 
                            m_httpsRequestRegexes[i].replace);
            break;
        }
    }

    if (store_entry.length() == 0) return;

    // Add/increment the entry.
    map<string, int>::iterator map_entry = m_httpsRequestEntries.find(store_entry);
    if (map_entry == m_httpsRequestEntries.end())
    {
        m_httpsRequestEntries[store_entry] = 1;
    }
    else
    {
        map_entry->second += 1;
    }
}

void LocalProxy::ParsePolipoStatsBuffer(const char* page_view_buffer)
{
    const char* HTTP_PREFIX = "PSIPHON-PAGE-VIEW-HTTP:>>";
    const char* HTTPS_PREFIX = "PSIPHON-PAGE-VIEW-HTTPS:>>";
    const char* BYTES_TRANSFERRED_PREFIX = "PSIPHON-BYTES-TRANSFERRED:>>";
    const char* DEBUG_PREFIX = "PSIPHON-DEBUG:>>";
    const char* ENTRY_END = "<<";

    const char* curr_pos = page_view_buffer;
    const char* end_pos = page_view_buffer + strlen(page_view_buffer);

    while (curr_pos < end_pos)
    {
        const char* http_entry_start = strstr(curr_pos, HTTP_PREFIX);
        const char* https_entry_start = strstr(curr_pos, HTTPS_PREFIX);
        const char* bytes_transferred_start = strstr(curr_pos, BYTES_TRANSFERRED_PREFIX);
        const char* debug_start = strstr(curr_pos, DEBUG_PREFIX);
        const char* entry_end = NULL;

        if (http_entry_start == NULL) http_entry_start = end_pos;
        if (https_entry_start == NULL) https_entry_start = end_pos;
        if (bytes_transferred_start == NULL) bytes_transferred_start = end_pos;
        if (debug_start == NULL) debug_start = end_pos;

        const char* next = min(http_entry_start, https_entry_start);
        next = min(next, bytes_transferred_start);
        next = min(next, debug_start);

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

            UpsertHttpsRequest(string(entry_start, entry_end-entry_start));
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

            int bytes = atoi(string(entry_start, entry_end-entry_start).c_str());
            if (bytes > 0)
            {
                m_bytesTransferred += bytes;
            }
        }
        else // if (next == debug_start)
        {
            const char* entry_start = next + strlen(DEBUG_PREFIX);
            entry_end = strstr(entry_start, ENTRY_END);

            if (!entry_end)
            {
                // Something is rather wrong. Maybe an incomplete entry.
                // Stop processing;
                break;
            }

            my_print(true, _T("POLIPO-DEBUG: %S"), string(entry_start, entry_end-entry_start).c_str());
        }

        curr_pos = entry_end + strlen(ENTRY_END);
    }
}

