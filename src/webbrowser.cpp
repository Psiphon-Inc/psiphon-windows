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


// Creates a process with the given command line and returns a HANDLE to the 
// resulting process. Returns 0 on error; call GetLastError to find out why.
HANDLE LaunchApplication(LPCTSTR command)
{
    STARTUPINFO startupInfo = {0};
    PROCESS_INFORMATION processInfo = {0};

    // The command argument is in-out, so we need to pass a modifiable buffer.
    int command_length = _tcslen(command) + 1; // includes the null terminator
    TCHAR *command_buffer = new TCHAR[command_length];

    try
    {
        _tcsncpy_s(command_buffer, command_length, command, command_length);

        if(::CreateProcess(NULL, 
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

// Launch the url in the default browser.
void LaunchWebPage(const tstring& url)
{
    HINSTANCE returnValue = ShellExecute(0, _T("open"), url.c_str(), 0, 0, SW_SHOWNORMAL);

    // If the function succeeds, it returns a value greater than 32. If the function fails,
    // it returns an error value that indicates the cause of the failure. 
    // http://msdn.microsoft.com/en-us/library/bb762153(v=vs.85).aspx
    if ((int)returnValue <= 32)
    {
        my_print(NOT_SENSITIVE, false, _T("ShellExecute failed (%d)"), (int)returnValue);
    }
}

void OpenBrowser(const tstring& url)
{
    vector<tstring> urls;
    urls.push_back(url);
    OpenBrowser(urls);
}

// Launch URLs in the default browser.
void OpenBrowser(const vector<tstring>& urls)
{
    vector<tstring>::const_iterator current_url = urls.begin();

    if (current_url == urls.end())
    {
        // No URLs to launch.
        return;
    }

    // Get the command line for the associated browser.

    TCHAR sBuffer[MAX_PATH]={0};
    DWORD dwSize = MAX_PATH;

    HRESULT hr = AssocQueryString(
        ASSOCF_INIT_DEFAULTTOSTAR, 
        ASSOCSTR_COMMAND,
        _T(".htm"),
        NULL,
        sBuffer,
        &dwSize);

    HANDLE hProcess = 0;
    if (hr == S_OK)
    {
        tstring command = sBuffer;

        // Replace the argument placeholder in the command line with the first URL
        // that we want to launch.

        tstring placeholder = _T("%1");
        size_t placeholder_pos = command.find(placeholder);
        if (placeholder_pos != tstring::npos)
        {
            command.replace(placeholder_pos, placeholder.length(), *current_url);
            ++current_url;
        }

        // Launch the application with the first URL.

        hProcess = LaunchApplication(command.c_str());

        if (hProcess == 0)
        {
            my_print(NOT_SENSITIVE, true, _T("LaunchApplication failed"));
            // But we'll continue anyway. Hopefully ShellExecute will still succeed.
        }
    }

    // Now that we're sure the application is open, launch the rest of the URLs.

    for (; current_url != urls.end(); ++current_url)
    {
        WaitForProcessToQuiesce(hProcess);

        LaunchWebPage(*current_url);
    }
}
