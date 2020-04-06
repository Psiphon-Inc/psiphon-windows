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
#include "logging.h"
#include "config.h"
#include "psiclient.h"
#include "webbrowser.h"
#include "shellapi.h"
#include "shlwapi.h"
#include "utilities.h"

extern HWND g_hWnd;

// Creates a process with the given command line and returns a HANDLE to the 
// resulting process. Returns 0 on error; call GetLastError to find out why.
HANDLE LaunchApplication(LPCTSTR command)
{
    STARTUPINFO startupInfo = { 0 };
    PROCESS_INFORMATION processInfo = { 0 };

    // The command argument is in-out, so we need to pass a modifiable buffer.
    int command_length = _tcslen(command) + 1; // includes the null terminator
    TCHAR *command_buffer = new TCHAR[command_length];

    try
    {
        _tcsncpy_s(command_buffer, command_length, command, command_length);

        if (::CreateProcess(
                NULL,
                command_buffer,
                NULL, NULL, FALSE, 0, NULL, NULL,
                &startupInfo, &processInfo))
        {
            delete[] command_buffer;
            return processInfo.hProcess;
        }
    }
    catch (...)
    {
        // Fall through to error condition
    }

    delete[] command_buffer;
    return 0;
}

// Wait for the browser to become available for more page launching.
// This is pretty much voodoo.
// hProcess should be a handle to the browser process, but can be 0.
void WaitForProcessToQuiesce(HANDLE hProcess)
{
    if (hProcess) ::WaitForInputIdle(hProcess, 10000);
    Sleep(2000);
    if (hProcess) ::WaitForInputIdle(hProcess, 10000);
}

// Get the command line for the default browser (should include URL placeholder,
// but that depends on what's configured in the system).
bool GetDefaultBrowserCommandLine(tstring& o_commandLine)
{
    o_commandLine.clear();

    // Get the command line for the associated browser.

    TCHAR sBuffer[MAX_PATH] = { 0 };
    DWORD dwSize = MAX_PATH;

    HRESULT hr = AssocQueryString(
        ASSOCF_INIT_DEFAULTTOSTAR,
        ASSOCSTR_COMMAND,
        _T(".htm"),
        NULL,
        sBuffer,
        &dwSize);

    if (hr != S_OK)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: AssocQueryString failed; hr:%d; GetLastError:%d"), __TFUNCTION__, (int)hr, GetLastError());
        return false;
    }

    o_commandLine = sBuffer;
    return true;
}

void OpenBrowser(const tstring& url)
{
    vector<tstring> urls;
    urls.push_back(url);
    OpenBrowser(urls);
}


void OpenBrowser(const vector<tstring>& urls)
{
    vector<tstring>::const_iterator currentURL = urls.begin();
    if (currentURL == urls.end())
    {
        // No URLs to launch.
        return;
    }

    HANDLE hProcess = 0;

    do { // not a real loop -- just to avoid using GOTO
        tstring browserCommandLine;
        if (!GetDefaultBrowserCommandLine(browserCommandLine))
        {
            my_print(NOT_SENSITIVE, true, _T("%s: GetDefaultBrowserCommandLine failed"), __TFUNCTION__);
            // Fall through to try out the other launching method
            break;
        }

        // Replace the argument placeholder in the command line with the first URL
        // that we want to launch.

        tstring placeholder = _T("%1");
        size_t placeholder_pos = browserCommandLine.find(placeholder);
        if (placeholder_pos == tstring::npos)
        {
            my_print(NOT_SENSITIVE, true, _T("%s: failed to find placeholder"), __TFUNCTION__);
            // Fall through to try out the other launching method
            break;
        }

        browserCommandLine.replace(placeholder_pos, placeholder.length(), *currentURL);

        // Launch the application with the first URL.

        hProcess = LaunchApplication(browserCommandLine.c_str());
        if (hProcess == 0)
        {
            my_print(NOT_SENSITIVE, true, _T("%s: LaunchApplication failed"), __TFUNCTION__);
            // Fall through to try out the other launching method
            break;
        }

        // Success. Advance the URL iterator.
        currentURL++;
    } while (false);

    // Either the browser is open now or we failed to open it and we're going to try other methods
    for (; currentURL != urls.end(); ++currentURL)
    {
        if (hProcess != 0)
        {
            // If the browser process just launched, we might need to wait for it
            // to quiet before opening more URLs.
            WaitForProcessToQuiesce(hProcess);
        }

        // First: Try to launch the URL normally, using the default browser.
        auto openRes = ShellExecute(g_hWnd, _T("open"), (*currentURL).c_str(), NULL, NULL, SW_SHOWNORMAL);
        // If the browser association is broken, the return value will be SE_ERR_NOASSOC.
        // We'll try "openas" no matter the error. (I've also seen SE_ERR_ACCESSDENIED 
        // with a NULL verb and broken association.)
        if ((int)openRes > 32)
        {
            // If ShellExecute succeeds, it returns a value greater than 32. If it fails,
            // it returns an error value that indicates the cause of the failure. 
            // http://msdn.microsoft.com/en-us/library/bb762153(v=vs.85).aspx

            // success
            continue;
        }
        my_print(NOT_SENSITIVE, true, _T("%s: ShellExecute-open failed: openRes:%d"), __TFUNCTION__, (int)openRes);

        // Second: Try to launch an "open with" dialog that will open a browser and 
        // help the user repair their broken https:// association.
        auto openAsRes = ShellExecute(g_hWnd, _T("openas"), (*currentURL).c_str(), NULL, NULL, SW_SHOWNORMAL);
        if ((int)openAsRes > 32)
        {
            // success
            continue;
        }
        my_print(NOT_SENSITIVE, true, _T("%s: ShellExecute-openas failed: openAsRes:%d"), __TFUNCTION__, (int)openAsRes);

        // Third and final: Create and execute a ".website" file, which will open Internet Explorer.
        // If this fails, we're not going to try to open the rest.
        tstring tempFilename;
        if (!GetUniqueTempFilename(_T(".website"), tempFilename))
        {
            my_print(NOT_SENSITIVE, true, _T("%s: GetUniqueTempFilename failed; GetLastError:%d"), __TFUNCTION__, (int)GetLastError());
            return;
        }

        std::stringstream fileContents;
        fileContents << "[InternetShortcut]\r\nURL=\"" << WStringToUTF8(*currentURL) << "\"\r\n";
        fileContents << "[{9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3}]\r\nProp5=8,Microsoft.Website.E2C499E3.EFF721D6\r\n";
        if (!WriteFile(tempFilename, fileContents.str()))
        {
            my_print(NOT_SENSITIVE, true, _T("%s: WriteFile failed"), __TFUNCTION__);
            return;
        }

        auto websiteFileRes = ShellExecute(g_hWnd, NULL, tempFilename.c_str(), NULL, NULL, SW_SHOWNORMAL);
        if ((int)websiteFileRes <= 32) {
            my_print(NOT_SENSITIVE, true, _T("%s: ShellExecute of .website failed: websiteFileRes:%d"), __TFUNCTION__, (int)websiteFileRes);
            return;
        }
    }
}
