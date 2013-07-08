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

/*
Wininet and Winhttp do no coexist peacefully together, so we need this separate
file that does not use the precompiled header in order to make Wininet calls.
*/

#include <windows.h>
#include <wininet.h>
#include "wininet_network_check.h"


bool WininetGetNetworkInfo(WininetNetworkInfo& netInfo)
{
    DWORD dwInternetConnectedFlags = 0;
    if (!InternetGetConnectedState(&dwInternetConnectedFlags, 0))
    {
        return false;
    }

    netInfo.internetConnectionConfigured = !!(dwInternetConnectedFlags & INTERNET_CONNECTION_CONFIGURED);
    netInfo.internetConnectionLAN = !!(dwInternetConnectedFlags & INTERNET_CONNECTION_LAN);
    netInfo.internetConnectionModem = !!(dwInternetConnectedFlags & INTERNET_CONNECTION_MODEM);
    netInfo.internetConnectionOffline = !!(dwInternetConnectedFlags & INTERNET_CONNECTION_OFFLINE);
    netInfo.internetConnectionProxy = !!(dwInternetConnectedFlags & INTERNET_CONNECTION_PROXY);
    netInfo.internetRASInstalled = !!(dwInternetConnectedFlags & INTERNET_RAS_INSTALLED);

    return true;
}
