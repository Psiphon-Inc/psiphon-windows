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
#include "config.h"
#include "psiclient.h"
#include "webbrowser.h"
#include "shellapi.h"

void OpenBrowser(const tstring& url)
{
    HINSTANCE returnValue = ShellExecute(0, _T("open"), url.c_str(), 0, 0, SW_SHOWNORMAL);

    // If the function succeeds, it returns a value greater than 32. If the function fails,
    // it returns an error value that indicates the cause of the failure. 
    // http://msdn.microsoft.com/en-us/library/bb762153(v=vs.85).aspx
    if ((int)returnValue <= 32)
    {
        my_print(false, _T("ShellExecute failed (%d)"), (int)returnValue);
    }
}

void OpenBrowser(const vector<string>& urls)
{
    for (vector<string>::const_iterator it = urls.begin();
         it != urls.end(); ++it)
    {
        OpenBrowser(NarrowToTString(*it));
    }
}
