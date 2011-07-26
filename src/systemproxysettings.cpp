/*
 * Copyright (c) 2010, Psiphon Inc.
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
#include "systemproxysettings.h"
#include "psiclient.h"
#include "wininet.h"
#include "config.h"
#include "ras.h"
#include "raserror.h"

SystemProxySettings::SystemProxySettings(void)
{
    m_originalSettings.clear();
}

SystemProxySettings::~SystemProxySettings(void)
{
    Revert();
}

bool SystemProxySettings::Configure(void)
{
    // Configure Windows Internet Settings to use our HTTP Proxy
    // This affects IE, Chrome, Safari and recent Firefox builds

    tstring proxyAddress = tstring(_T("127.0.0.1:")) + POLIPO_HTTP_PROXY_PORT;

    // Get a list of connections, starting with the dial-up connections
    vector<tstring> connections = GetRasConnectionNames();
    
    // NULL indicates the default or LAN connection
    connections.push_back(_T(""));

    return (Save(connections) && SetConnectionsProxies(connections, proxyAddress));
}

bool SystemProxySettings::Revert(void)
{
    // Revert Windows Internet Settings back to user's original configuration

    bool success = true;

    for (connection_proxy_iter ii = m_originalSettings.begin();
         ii != m_originalSettings.end();
         ++ii)
    {
        if (!SetConnectionProxy(*ii))
        {
            success = false;
            break;
        }
    }

    if (success)
    {
        m_originalSettings.clear();
    }

    return success;
}

// TODO: do this properly.
// ie. Save original proxy settings to a file, and remove the file on restoring settings.
// If the file exists when saving original settings, use the file's contents and don't save
// the existing settings.
void SystemProxySettings::PreviousCrashCheckHack(connection_proxy& proxySettings)
{
    tstring proxyAddress = tstring(_T("127.0.0.1:")) + POLIPO_HTTP_PROXY_PORT;

    // Don't save settings that are the same as we will be setting.
    // Instead, save default (no proxy) settings.
    if (   (proxySettings.flags == PROXY_TYPE_PROXY)
        && (proxySettings.proxy == proxyAddress))
    {
        proxySettings.flags = PROXY_TYPE_DIRECT;
        proxySettings.proxy = L"";
    }
}

bool SystemProxySettings::Save(const vector<tstring>& connections)
{
    if (!m_originalSettings.empty())
    {
        my_print(false, _T("Error: can't save Proxy Settings because they are already saved."));
        my_print(false, _T("Original proxy settings may not be restored correctly."));
        return false;
    }

    bool success = true;
    connection_proxy proxySettings;

    for (tstring_iter ii = connections.begin();
         ii != connections.end();
         ++ii)
    {
        proxySettings.name = *ii;
        if (GetConnectionProxy(proxySettings))
        {
            PreviousCrashCheckHack(proxySettings);
            m_originalSettings.push_back(proxySettings);
        }
        else
        {
            success = false;
            break;
        }
    }

    if (!success)
    {
        // If we failed to save any connection, discard everything.
        // Nothing should proceed.
        m_originalSettings.clear();
    }

    return success;
}

bool SystemProxySettings::SetConnectionsProxies(const vector<tstring>& connections, const tstring& proxyAddress)
{
    bool success = true;
    connection_proxy proxySettings;

    for (tstring_iter ii = connections.begin();
         ii != connections.end();
         ++ii)
    {
        // These are the new proxy settings we want to use
        proxySettings.name = *ii;
        proxySettings.flags = PROXY_TYPE_PROXY;
        proxySettings.proxy = proxyAddress;

        if (!SetConnectionProxy(proxySettings))
        {
            success = false;
            break;
        }
    }

    return success;
}

bool SystemProxySettings::SetConnectionProxy(const connection_proxy& setting)
{
    INTERNET_PER_CONN_OPTION_LIST list;
    INTERNET_PER_CONN_OPTION options[INTERNET_OPTIONS_NUMBER];
    list.dwSize = sizeof(list);
    // Pointer to a string that contains the name of the RAS connection
    // or NULL, which indicates the default or LAN connection, to set or query options on.
    list.pszConnection = setting.name.length() ? const_cast<TCHAR*>(setting.name.c_str()) : 0;
    list.dwOptionCount = sizeof(options)/sizeof(INTERNET_PER_CONN_OPTION);
    list.pOptions = options;

    list.pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS;
    list.pOptions[0].Value.dwValue = setting.flags;

    list.pOptions[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    list.pOptions[1].Value.pszValue = const_cast<TCHAR*>(setting.proxy.c_str());

    bool success = (0 != InternetSetOption(0, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, list.dwSize));

    if (success)
    {
        InternetSetOption(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
        InternetSetOption(NULL, INTERNET_OPTION_REFRESH , NULL, 0);
    }
    else
    {
        my_print(false, _T("InternetSetOption error: %d"), GetLastError());
        // NOTE: We are calling the Unicode version of InternetSetOption.
        // In Microsoft Internet Explorer 5, only the ANSI versions of InternetQueryOption and InternetSetOption
        // will work with the INTERNET_PER_CONN_OPTION_LIST structure.
    }

    return success;
}

bool SystemProxySettings::GetConnectionProxy(connection_proxy& setting)
{
    INTERNET_PER_CONN_OPTION_LIST list;
    INTERNET_PER_CONN_OPTION options[INTERNET_OPTIONS_NUMBER];
    unsigned long length = sizeof(list);
    list.dwSize = length;
    // Pointer to a string that contains the name of the RAS connection
    // or NULL, which indicates the default or LAN connection, to set or query options on.
    list.pszConnection = setting.name.length() ? const_cast<TCHAR*>(setting.name.c_str()) : 0;
    list.dwOptionCount = sizeof(options)/sizeof(INTERNET_PER_CONN_OPTION);
    list.pOptions = options;

    options[0].dwOption = INTERNET_PER_CONN_FLAGS;
    options[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;

    if (0 == InternetQueryOption(0, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, &length))
    {
        my_print(false, _T("InternetQueryOption error: %d"), GetLastError());
        // NOTE: We are calling the Unicode version of InternetQueryOption.
        // In Microsoft Internet Explorer 5, only the ANSI versions of InternetQueryOption and InternetSetOption
        // will work with the INTERNET_PER_CONN_OPTION_LIST structure.
        return false;
    }

    setting.flags = options[0].Value.dwValue;
    setting.proxy = options[1].Value.pszValue ? options[1].Value.pszValue : _T("");

    // Cleanup
    if (options[1].Value.pszValue)
    {
        GlobalFree(options[1].Value.pszValue);
    }

    return true;
}

static DWORD GetRasEntries(LPRASENTRYNAME& rasEntryNames, DWORD& bufferSize, DWORD& entries)
{
    if (rasEntryNames)
    {
        // Deallocate memory for the entries buffer
        HeapFree(GetProcessHeap(), 0, rasEntryNames);
        rasEntryNames = 0;
    }

    rasEntryNames = (LPRASENTRYNAME)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufferSize);

    if (!rasEntryNames)
    {
        my_print(false, _T("HeapAlloc failed when trying to enumerate RAS connections"));
        throw 0;
    }

    // The first RASENTRYNAME structure in the array must contain the structure size
    rasEntryNames[0].dwSize = sizeof(RASENTRYNAME);

    // Call RasEnumEntries to enumerate all RAS entry names
    return RasEnumEntries(0, 0, rasEntryNames, &bufferSize, &entries);
}

vector<tstring> SystemProxySettings::GetRasConnectionNames(void)
{
    vector<tstring> connections;
    LPRASENTRYNAME rasEntryNames = 0;
    // The RasEnumEntries API requires that we pass in a buffer first
    // and if the buffer is too small, it tells us how big a buffer it needs.
    // For the first call we will pass in a single RASENTRYNAME struct.
    DWORD bufferSize = sizeof(RASENTRYNAME);
    DWORD entries = 0;
    DWORD returnCode = ERROR_SUCCESS;

    try
    {
        returnCode = GetRasEntries(rasEntryNames, bufferSize, entries);

        if (ERROR_BUFFER_TOO_SMALL == returnCode)
        {
            returnCode = GetRasEntries(rasEntryNames, bufferSize, entries);
        }

        if (ERROR_SUCCESS != returnCode)
        {
            my_print(false, _T("failed to enumerate RAS connections (%d)"), returnCode);
            throw 0;
        }

        for (DWORD i = 0; i < entries; i++)
        {
            connections.push_back(rasEntryNames[i].szEntryName);
	    }
    }
    catch (...)
    {
    }
    // TODO: Don't throw 0.  Throw and catch a std::exception.
    // Maybe put the error message in the exception.

    // Clean up

    if (rasEntryNames)
    {
        //Deallocate memory for the entries buffer
        HeapFree(GetProcessHeap(), 0, rasEntryNames);
        rasEntryNames = 0;
    }

    return connections;
}
