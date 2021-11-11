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

// Adapted from the MSDN htmldlg demo. Available here: http://www.microsoft.com/en-us/download/details.aspx?id=944

#include "stdafx.h"
#include "htmldlg.h"
#include "utilities.h"


/**************************************************************************
ResourceToUrl()
**************************************************************************/
tstring ResourceToUrl(LPCTSTR resourceName, LPCTSTR urlQuery, LPCTSTR urlFragment)
{
    tstring url;

    url += _T("res://");

    tstring exePath;
    (void)GetOwnExecutablePath(exePath);
    url += exePath;

    url += _T("/");
    url += resourceName;

    // URI encoding seems to be taken care of automatically (fortuitous, but unsettling)

    if (urlQuery)
    {
        url += _T("?");
        url += urlQuery;
    }

    if (urlFragment)
    {
        url += _T("#");
        url += urlFragment;
    }

    return url;
}
