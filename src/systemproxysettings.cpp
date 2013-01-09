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
#include "systemproxysettings.h"
#include "psiclient.h"
#include "wininet.h"
#include "config.h"
#include "ras.h"
#include "raserror.h"
#include "usersettings.h"
#include "utilities.h"


void AddOriginalProxyInfo(const connection_proxy& proxyInfo);
bool GetConnectionsAndProxyInfo(vector<tstring>& connections, vector<connection_proxy>& proxyInfo);
bool SetConnectionProxy(const connection_proxy& setting);


static const TCHAR* SYSTEM_PROXY_SETTINGS_PROXY_BYPASS = _T("<local>");
static const int INTERNET_OPTIONS_NUMBER = 3;


struct connection_proxy
{
    tstring name;
    DWORD flags;
    tstring proxy;
    tstring bypass;

    bool operator==(const connection_proxy& rhs)
    {
        return 
            this->name == rhs.name &&
            this->flags == rhs.flags &&
            this->proxy == rhs.proxy &&
            this->bypass == rhs.bypass;
    }
};


SystemProxySettings::SystemProxySettings()
    : m_settingsApplied(false)
{
    m_originalSettings.clear();
    SetHttpProxyPort(0);
    SetHttpsProxyPort(0);
    SetSocksProxyPort(0);
}

SystemProxySettings::~SystemProxySettings()
{
    Revert();
}

void SystemProxySettings::SetHttpProxyPort(int port)
{
    m_httpProxyPort = port;
}
void SystemProxySettings::SetHttpsProxyPort(int port)
{
    m_httpsProxyPort = port;
}
void SystemProxySettings::SetSocksProxyPort(int port)
{
    m_socksProxyPort = port;
}

bool SystemProxySettings::Apply()
{
    if (UserSkipProxySettings())
    {
        return false;
    }

    // Configure Windows Internet Settings to use our HTTP Proxy
    // This affects IE, Chrome, Safari and recent Firefox builds

    tstring proxyAddress = MakeProxySettingString();
    if(proxyAddress.length() == 0)
    {
        return false;
    }

    m_settingsApplied = true;

    vector<tstring> connections;
    vector<connection_proxy> proxyInfo;
    if (!GetConnectionsAndProxyInfo(connections, proxyInfo))
    {
        return false;
    }
    
    if (!Save(proxyInfo))
    {
        return false;
    }

    if (!SetConnectionsProxies(connections, proxyAddress))
    {
        return false;
    }
    
    return true;
}

tstring SystemProxySettings::MakeProxySettingString()
{
    // This string is passed to InternetSetOption to set the proxy address for each protocol.
    // NOTE that we do not include a proxy setting for FTP, since Polipo does not support
    // proxying FTP, so FTP will not be proxied.

    tstringstream proxySetting;

    if (m_httpProxyPort > 0)
    {
        if (proxySetting.str().length() > 0) proxySetting << _T(";");
        proxySetting << _T("http=127.0.0.1:") << m_httpProxyPort;
    }

    if (m_httpsProxyPort > 0)
    {
        if (proxySetting.str().length() > 0) proxySetting << _T(";");
        proxySetting << _T("https=127.0.0.1:") << m_httpsProxyPort;
    }

    if (m_socksProxyPort > 0)
    {
        if (proxySetting.str().length() > 0) proxySetting << _T(";");
        proxySetting << _T("socks=127.0.0.1:") << m_socksProxyPort;
    }

    return proxySetting.str();
}

bool SystemProxySettings::Revert()
{
    // Revert Windows Internet Settings back to user's original configuration

    if (!m_settingsApplied)
    {
        return true;
    }

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
    tstring proxyAddress = MakeProxySettingString();

    // Don't save settings that are the same as we will be setting.
    // Instead, save default (no proxy) settings.
    if (   (proxySettings.flags == PROXY_TYPE_PROXY)
        && (proxySettings.proxy == proxyAddress)
        && (proxySettings.bypass == SYSTEM_PROXY_SETTINGS_PROXY_BYPASS))
    {
        proxySettings.flags = PROXY_TYPE_DIRECT;
        proxySettings.proxy = L"";
        proxySettings.bypass = L"";
    }
}

bool SystemProxySettings::Save(const vector<connection_proxy>& proxyInfo)
{
    if (!m_originalSettings.empty())
    {
        my_print(NOT_SENSITIVE, false, _T("Error: can't save Proxy Settings because they are already saved."));
        my_print(NOT_SENSITIVE, false, _T("Original proxy settings may not be restored correctly."));
        return false;
    }

    bool success = true;
    connection_proxy proxySettings;

    for (vector<connection_proxy>::const_iterator ii = proxyInfo.begin();
         ii != proxyInfo.end();
         ++ii)
    {
        connection_proxy proxySettings = *ii;
        PreviousCrashCheckHack(proxySettings);
        m_originalSettings.push_back(proxySettings);
    }

    return true;
}

bool SystemProxySettings::SetConnectionsProxies(const vector<tstring>& connections, const tstring& proxyAddress)
{
    bool success = true;

    for (vector<tstring>::const_iterator ii = connections.begin();
         ii != connections.end();
         ++ii)
    {
        connection_proxy proxySettings;
        // These are the new proxy settings we want to use
        proxySettings.name = *ii;
        proxySettings.flags = PROXY_TYPE_PROXY;
        proxySettings.proxy = proxyAddress;
        proxySettings.bypass = SYSTEM_PROXY_SETTINGS_PROXY_BYPASS;

        if (!SetConnectionProxy(proxySettings))
        {
            success = false;
            break;
        }
    }

    return success;
}


/**********************************************************
*
* Proxy settings helpers
*
**********************************************************/

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
        my_print(NOT_SENSITIVE, false, _T("HeapAlloc failed when trying to enumerate RAS connections"));
        throw 0;
    }

    // The first RASENTRYNAME structure in the array must contain the structure size
    rasEntryNames[0].dwSize = sizeof(RASENTRYNAME);

    // Call RasEnumEntries to enumerate all RAS entry names
    return RasEnumEntries(0, 0, rasEntryNames, &bufferSize, &entries);
}


vector<tstring> GetRasConnectionNames()
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
            NOT_SENSITIVE, (false, _T("failed to enumerate RAS connections (%d)"), returnCode);
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


bool SetConnectionProxy(const connection_proxy& setting)
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

    list.pOptions[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
    list.pOptions[2].Value.pszValue = const_cast<TCHAR*>(setting.bypass.c_str());

    bool success = (0 != InternetSetOption(0, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, list.dwSize));

    if (success)
    {
        InternetSetOption(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
        InternetSetOption(NULL, INTERNET_OPTION_REFRESH , NULL, 0);
    }
    else
    {
        my_print(NOT_SENSITIVE, false, _T("InternetSetOption error: %d"), GetLastError());
        // NOTE: We are calling the Unicode version of InternetSetOption.
        // In Microsoft Internet Explorer 5, only the ANSI versions of InternetQueryOption and InternetSetOption
        // will work with the INTERNET_PER_CONN_OPTION_LIST structure.
    }

    return success;
}

bool GetConnectionProxy(connection_proxy& setting)
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
    options[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;

    if (0 == InternetQueryOption(0, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, &length))
    {
        my_print(NOT_SENSITIVE, false, _T("InternetQueryOption error: %d"), GetLastError());
        // NOTE: We are calling the Unicode version of InternetQueryOption.
        // In Microsoft Internet Explorer 5, only the ANSI versions of InternetQueryOption and InternetSetOption
        // will work with the INTERNET_PER_CONN_OPTION_LIST structure.
        return false;
    }

    setting.flags = options[0].Value.dwValue;
    setting.proxy = options[1].Value.pszValue ? options[1].Value.pszValue : _T("");
    setting.bypass = options[2].Value.pszValue ? options[2].Value.pszValue : _T("");

    // Cleanup
    if (options[1].Value.pszValue)
    {
        GlobalFree(options[1].Value.pszValue);
    }
    if (options[2].Value.pszValue)
    {
        GlobalFree(options[2].Value.pszValue);
    }

    return true;
}


bool GetConnectionsAndProxyInfo(vector<tstring>& connections, vector<connection_proxy>& proxyInfo)
{
    connections.clear();
    proxyInfo.clear();

    // Get a list of connections, starting with the dial-up connections
    connections = GetRasConnectionNames();
    
    // NULL indicates the default or LAN connection
    connections.push_back(_T(""));

    for (vector<tstring>::const_iterator ii = connections.begin();
         ii != connections.end();
         ++ii)
    {
        connection_proxy entry;

        entry.name = *ii;
        if (GetConnectionProxy(entry))
        {
            proxyInfo.push_back(entry);
            AddOriginalProxyInfo(entry);
        }
        else
        {
            connections.clear();
            proxyInfo.clear();

            return false;
        }
    }

    return true;
}


/**********************************************************
*
* Original proxy info for use with diagnostic feedback info
*
**********************************************************/

HANDLE g_originalProxyInfoMutex = CreateMutex(NULL, FALSE, 0);
vector<connection_proxy> g_originalProxyInfo;

/**
Returns a santized/de-personalized copy of the original proxy info.
*/
void GetOriginalProxyInfo(vector<ConnectionProxyInfo>& originalProxyInfo)
{
    originalProxyInfo.clear();

    vector<connection_proxy> rawOriginalProxyInfo;
    // Only grab the mutex temporarily.
    {
        AutoMUTEX mutex(g_originalProxyInfoMutex);
        rawOriginalProxyInfo = g_originalProxyInfo;
    }

    // This might be called before SystemProxySettings has filled in the 
    // desired values (which only happens after a successful connection).
    // In that case, we'll get them here.
    if (rawOriginalProxyInfo.empty())
    {
        if (!GetConnectionsAndProxyInfo(vector<tstring>(), rawOriginalProxyInfo))
        {
            return;
        }
    }

    /*
    De-personalize system proxy server info
    */

    // ASSUMPTION: Proxy entries will always have a port number.
    // There are a number of forms (of interest to us) that a proxy server entry can take:
    // localhost or 127.0.0.1
    basic_regex<TCHAR> localhost_regex = basic_regex<TCHAR>(
                    _T("^([\\w]+=)?([a-z]+:\\/\\/)?(?:(?:localhost)|(?:127\\.0\\.0\\.1))(:[0-9]+)$"), 
                    regex::ECMAScript | regex::icase);
    // IPv4 (Note: very rough, but probably good enough)
    basic_regex<TCHAR> ipv4_regex = basic_regex<TCHAR>(
                    _T("^([\\w]+=)?([a-z]+:\\/\\/)?(?:\\d+\\.\\d+\\.\\d+\\.\\d+)(:[0-9]+)$"), 
                    regex::ECMAScript | regex::icase);
    // IPv6 (Note: also very rough but probably good enough)
    basic_regex<TCHAR> ipv6_regex = basic_regex<TCHAR>(
                    _T("^([\\w]+=)?([a-z]+:\\/\\/)?(?:\\[[a-fA-F0-9:]+\\])(:[0-9]+)$"), 
                    regex::ECMAScript | regex::icase);
    // not-fully-qualified domain name
    basic_regex<TCHAR> nonfqdn_regex = basic_regex<TCHAR>(
                    _T("^([\\w]+=)?([a-z]+:\\/\\/)?(?:[\\w\\-]+)(:[0-9]+)$"), 
                    regex::ECMAScript | regex::icase);
    // fully qualified domain name
    basic_regex<TCHAR> fqdn_regex = basic_regex<TCHAR>(
                    _T("^([\\w]+=)?([a-z]+:\\/\\/)?(?:[\\w\\-\\.]+)(:[0-9]+)$"), 
                    regex::ECMAScript | regex::icase);

    for (vector<connection_proxy>::iterator it = rawOriginalProxyInfo.begin();
         it != rawOriginalProxyInfo.end();
         it++)
    {
        ConnectionProxyInfo info;

        // We can't usefully redact this value. Maybe it shouldn't be included?
        if (it->name.empty())
        {
            info.connectionName = _T("");
        }
        else
        {
            info.connectionName = _T("[REDACTED]");
        }

        vector<tstring> proxyElems = split<TCHAR>(it->proxy, _T(';'));
        tstringstream ss;
        for (vector<tstring>::iterator elem = proxyElems.begin();
             elem != proxyElems.end();
             elem++)
        {
            if (elem != proxyElems.begin())
            {
                ss << _T(";");
            }

            if (regex_match(*elem, localhost_regex))
            {
                ss << regex_replace(*elem, localhost_regex, _T("$1[LOCALHOST]$2$3"));
            }
            else if (regex_match(*elem, ipv4_regex))
            {
                ss << regex_replace(*elem, ipv4_regex, _T("$1[IPV4]$2$3"));
            }
            else if (regex_match(*elem, ipv6_regex))
            {
                ss << regex_replace(*elem, ipv6_regex, _T("$1[IPV6]$2$3"));
            }
            else if (regex_match(*elem, nonfqdn_regex))
            {
                ss << regex_replace(*elem, nonfqdn_regex, _T("$1[NONFQDN]$2$3"));
            }
            else if (regex_match(*elem, fqdn_regex))
            {
                ss << regex_replace(*elem, fqdn_regex, _T("$1[FQDN]$2$3"));
            }
            else
            {
                ss << _T("[UNMATCHED]");
            }
        }

        info.proxy = ss.str();

        /*
        De-personalize system proxy bypass info
        */

        ss.str(_T(""));
        ss.clear();

        vector<tstring> bypassElems = split<TCHAR>(it->bypass, _T(';'));
        for (vector<tstring>::iterator elem = bypassElems.begin();
             elem != bypassElems.end();
             elem++)
        {
            if (*elem == SYSTEM_PROXY_SETTINGS_PROXY_BYPASS)
            {
                ss << SYSTEM_PROXY_SETTINGS_PROXY_BYPASS << ";";
            }
            else
            {
                ss << "[REDACTED];";
            }
        }

        info.bypass = ss.str();

        /*
        Make proxy flags human-readable
        */

        ss.str(_T(""));
        ss.clear();

        if (it->flags & PROXY_TYPE_DIRECT)
        {
            ss << _T("PROXY_TYPE_DIRECT|");
        }
        else if (it->flags & PROXY_TYPE_PROXY)
        {
            ss << _T("PROXY_TYPE_PROXY|");
        }
        else if (it->flags & PROXY_TYPE_AUTO_PROXY_URL)
        {
            ss << _T("PROXY_TYPE_AUTO_PROXY_URL|");
        }
        else if (it->flags & PROXY_TYPE_AUTO_DETECT)
        {
            ss << _T("PROXY_TYPE_AUTO_DETECT|");
        }        

        info.flags = ss.str();
        if (info.flags.length() > 0)
        {
            // Strip the trailing "|"
            info.flags.resize(info.flags.size()-1);
        }

        originalProxyInfo.push_back(info);
    }
}

void AddOriginalProxyInfo(const connection_proxy& proxyInfo)
{
    AutoMUTEX mutex(g_originalProxyInfoMutex);

    vector<connection_proxy>::iterator match = find(g_originalProxyInfo.begin(), g_originalProxyInfo.end(), proxyInfo);
    if (match == g_originalProxyInfo.end())
    {
        // Entry doesn't already exist in vector
        g_originalProxyInfo.push_back(proxyInfo);
    }
}
