/*
 * Copyright (c) 2013, Psiphon Inc.
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
#include "logging.h"
#include "psiclient.h"
#include "wininet.h"
#include "config.h"
#include "ras.h"
#include "raserror.h"
#include "usersettings.h"
#include "utilities.h"


static const TCHAR* DEFAULT_CONNECTION_NAME = _T("");
static const TCHAR* SYSTEM_PROXY_SETTINGS_PROXY_BYPASS = _T("<local>");
static const int INTERNET_OPTIONS_NUMBER = 3;

bool GetCurrentSystemConnectionsProxyInfo(vector<ConnectionProxy>& o_proxyInfo);
bool GetCurrentSystemConnectionProxy(tstring connectionName, ConnectionProxy& o_proxyInfo);
bool SetCurrentSystemConnectionsProxy(const vector<ConnectionProxy>& connectionsProxies);
void SetPsiphonProxyForConnections(vector<ConnectionProxy>& io_connectionsProxies, 
                                   const tstring& psiphonProxyAddress);
void ClearRegistryProxyInfo(const char* regKey);
void ReadRegistryProxyInfo(const char* regKey, vector<ConnectionProxy>& o_proxyInfo);
void WriteRegistryProxyInfo(const char* regKey, const vector<ConnectionProxy>& proxyInfo);


SystemProxySettings::SystemProxySettings()
    : m_settingsApplied(false)
{
    SetHttpProxyPort(0);
    SetHttpsProxyPort(0);
    SetSocksProxyPort(0);
}

SystemProxySettings::~SystemProxySettings()
{
    // This Revert() cannot be called unconditionally here,
    // because URL Proxy TransportConnections should not result
    // in reverting System Proxy Settings.
    // See TransportConnection.m_skipApplySystemProxySettings
    // SystemProxySettings::Revert() is explicitly called by
    // TransportConnection::Cleanup()
    // And nowhere else do we rely on SystemProxySettings' dtor
    // for reverting.
    //Revert();
}

int SystemProxySettings::GetHttpProxyPort() const
{
    return m_httpProxyPort;
}

int SystemProxySettings::GetHttpsProxyPort() const
{
    return m_httpsProxyPort;
}

int SystemProxySettings::GetSocksProxyPort() const
{
    return m_socksProxyPort;
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

bool SystemProxySettings::Apply(bool allowedToSkipProxySettings)
{
    // Configure Windows Internet Settings to use our HTTP Proxy
    // This affects IE, Chrome, Safari and recent Firefox builds

    assert(!m_settingsApplied);

    tstring psiphonProxyAddress = MakeProxySettingString();
    if(psiphonProxyAddress.length() == 0)
    {
        return false;
    }

    vector<ConnectionProxy> proxyInfo;
    if (!GetCurrentSystemConnectionsProxyInfo(proxyInfo))
    {
        return false;
    }

    SetPsiphonProxyForConnections(proxyInfo, psiphonProxyAddress);
    WriteRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_PSIPHON_PROXY_INFO, proxyInfo);

    if (allowedToSkipProxySettings && Settings::SkipProxySettings())
    {
        return true;
    }

    if (!SetCurrentSystemConnectionsProxy(proxyInfo))
    {
        return false;
    }
    
    m_settingsApplied = true;

    return true;
}

tstring SystemProxySettings::MakeProxySettingString() const
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
        ClearRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_PSIPHON_PROXY_INFO);
        return true;
    }

    vector<ConnectionProxy> originalProxySettings;
    ReadRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_NATIVE_PROXY_INFO, originalProxySettings);
    
    bool success = SetCurrentSystemConnectionsProxy(originalProxySettings);

    if (success)
    {
        m_settingsApplied = false;
        // Only clear this if we successfully restored the original System Proxy Settings,
        // since this is used to determine on subsequent runs whether to restore original System
        // Proxy Settings first.
        ClearRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_PSIPHON_PROXY_INFO);
    }

    return success;
}

bool SystemProxySettings::IsApplied() const
{
    return m_settingsApplied;
}


/**********************************************************
*
* Proxy settings helpers
*

Terminology:
    native: the system proxy settings before Psiphon runs
    default: the default connection (there might be a lot of system connections with different proxies)

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
            my_print(NOT_SENSITIVE, false, _T("failed to enumerate RAS connections (%d)"), returnCode);
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


bool SetCurrentSystemConnectionProxy(const ConnectionProxy& setting)
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

    bool success = (0 != InternetSetOption(0, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, list.dwSize)) &&
                   (0 != InternetSetOption(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0)) &&
                   (0 != InternetSetOption(NULL, INTERNET_OPTION_REFRESH , NULL, 0));
    
    if (!success)
    {
        my_print(NOT_SENSITIVE, false, _T("InternetSetOption error: %d"), GetLastError());
        // NOTE: We are calling the Unicode version of InternetSetOption.
        // In Microsoft Internet Explorer 5, only the ANSI versions of InternetQueryOption and InternetSetOption
        // will work with the INTERNET_PER_CONN_OPTION_LIST structure.
    }

    return success;
}


bool SetCurrentSystemConnectionsProxy(const vector<ConnectionProxy>& connectionsProxies)
{
    bool success = true;

    for (vector<ConnectionProxy>::const_iterator ii = connectionsProxies.begin();
         ii != connectionsProxies.end();
         ++ii)
    {
        if (!SetCurrentSystemConnectionProxy(*ii))
        {
            success = false;
            break;
        }


        // Read back the settings to verify that they have been applied
        ConnectionProxy entry;
        if (!GetCurrentSystemConnectionProxy(ii->name, entry) ||
            entry != *ii)
        {
            if (ii->name.empty())
            {
                // This is the default or LAN connection.
                UI_Notice("SystemProxySettings::SetProxyError", "");
                my_print(NOT_SENSITIVE, false, _T("%s:%d: failed to verify proxy setting for default connection"), __TFUNCTION__, __LINE__);
                success = false;
                break;
            }
            else
            {
                // Don't force the connection to fail, this might not be an active connection.
                UI_Notice("SystemProxySettings::SetProxyWarning", WStringToUTF8(ii->name));
                my_print(NOT_SENSITIVE, false, _T("%s:%d: failed to verify proxy setting for non-default connection: %s"), __TFUNCTION__, __LINE__, ii->name.c_str());
            }
        }
    }

    return success;
}


bool GetCurrentSystemConnectionProxy(tstring connectionName, ConnectionProxy& o_proxyInfo)
{
    o_proxyInfo.clear();

    INTERNET_PER_CONN_OPTION_LIST list;
    INTERNET_PER_CONN_OPTION options[INTERNET_OPTIONS_NUMBER];
    unsigned long length = sizeof(list);
    list.dwSize = length;
    // Pointer to a string that contains the name of the RAS connection
    // or NULL, which indicates the default or LAN connection, to set or query options on.
    list.pszConnection = connectionName.length() ? const_cast<TCHAR*>(connectionName.c_str()) : 0;
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

    o_proxyInfo.name = connectionName;
    o_proxyInfo.flags = options[0].Value.dwValue;
    o_proxyInfo.proxy = options[1].Value.pszValue ? options[1].Value.pszValue : _T("");
    o_proxyInfo.bypass = options[2].Value.pszValue ? options[2].Value.pszValue : _T("");

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

bool GetCurrentSystemConnectionsProxyInfo(vector<ConnectionProxy>& o_proxyInfo)
{
    o_proxyInfo.clear();

    vector<tstring> connections;

    // Get a list of connections, starting with the dial-up connections
    connections = GetRasConnectionNames();
    
    // NULL indicates the default or LAN connection
    connections.push_back(DEFAULT_CONNECTION_NAME);

    for (vector<tstring>::const_iterator ii = connections.begin();
         ii != connections.end();
         ++ii)
    {
        ConnectionProxy entry;

        if (GetCurrentSystemConnectionProxy(*ii, entry))
        {
            o_proxyInfo.push_back(entry);
        }
        else
        {
            o_proxyInfo.clear();
            return false;
        }
    }

    return true;
}


void SetPsiphonProxyForConnections(vector<ConnectionProxy>& io_connectionsProxies, 
                                   const tstring& psiphonProxyAddress)
{
    for (vector<ConnectionProxy>::iterator ii = io_connectionsProxies.begin();
         ii != io_connectionsProxies.end();
         ++ii)
    {
        // These are the new proxy settings we want to use
        ii->flags = PROXY_TYPE_PROXY;
        ii->proxy = psiphonProxyAddress;
        ii->bypass = SYSTEM_PROXY_SETTINGS_PROXY_BYPASS;
    }
}


void GetDefaultProxyInfo(const vector<ConnectionProxy>& proxyInfos, ConnectionProxy& o_defaultProxyInfo)
{
    o_defaultProxyInfo.clear();

    for (vector<ConnectionProxy>::const_iterator ii = proxyInfos.begin();
         ii != proxyInfos.end();
         ++ii)
    {
        if (ii->name == DEFAULT_CONNECTION_NAME)
        {
            o_defaultProxyInfo = *ii;
            return;
        }
    }
}

void DecomposeDefaultProxyInfo(const ConnectionProxy& proxyInfo, DecomposedProxyConfig& o_proxyConfig);

/**
Get the proxy info for the original default connection.
*/
void GetNativeDefaultProxyInfo(ConnectionProxy& o_proxyInfo)
{
    o_proxyInfo.clear();

    vector<ConnectionProxy> proxyInfo;
    ReadRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_NATIVE_PROXY_INFO, proxyInfo);

    GetDefaultProxyInfo(proxyInfo, o_proxyInfo);
}

void GetNativeDefaultProxyInfo(DecomposedProxyConfig& o_proxyInfo)
{
    o_proxyInfo.clear();

    vector<ConnectionProxy> proxyInfo;
    ReadRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_NATIVE_PROXY_INFO, proxyInfo);

    ConnectionProxy undecomposedProxyInfo;
    GetDefaultProxyInfo(proxyInfo, undecomposedProxyInfo);

    DecomposeDefaultProxyInfo(undecomposedProxyInfo, o_proxyInfo);
}

tstring GetNativeDefaultHttpsProxyHost()
{
    ConnectionProxy proxyInfo;
    GetNativeDefaultProxyInfo(proxyInfo);
    
    DecomposedProxyConfig decomposedProxyConfig;
    DecomposeDefaultProxyInfo(proxyInfo, decomposedProxyConfig);

    tstringstream host;
    host << decomposedProxyConfig.httpsProxy;
    if (!host.str().empty() && decomposedProxyConfig.httpsProxyPort != 0)
    {
        host << _T(":") << decomposedProxyConfig.httpsProxyPort;
    }

    return host.str();
}


void GetTunneledDefaultProxyInfo(ConnectionProxy& o_proxyInfo)
{
    o_proxyInfo.clear();

    vector<ConnectionProxy> proxyInfo;
    ReadRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_PSIPHON_PROXY_INFO, proxyInfo);

    GetDefaultProxyInfo(proxyInfo, o_proxyInfo);
}


tstring GetTunneledDefaultHttpsProxyHost()
{
    ConnectionProxy proxyInfo;
    GetTunneledDefaultProxyInfo(proxyInfo);
    
    DecomposedProxyConfig decomposedProxyConfig;
    DecomposeDefaultProxyInfo(proxyInfo, decomposedProxyConfig);

    tstringstream host;
    host << decomposedProxyConfig.httpsProxy;
    if (!host.str().empty() && decomposedProxyConfig.httpsProxyPort != 0)
    {
        host << _T(":") << decomposedProxyConfig.httpsProxyPort;
    }

    return host.str();
}


/**
Returns a santized/de-personalized copy of the original proxy info.
*/
void GetSanitizedOriginalProxyInfo(vector<ConnectionProxy>& o_originalProxyInfo)
{
    vector<ConnectionProxy> rawOriginalProxyInfo;
    ReadRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_NATIVE_PROXY_INFO, rawOriginalProxyInfo);

    /*
    De-personalize system proxy server info
    */

    // ASSUMPTION: Proxy entries will always have a port number.
    // There are a number of forms (of interest to us) that a proxy server entry can take:
    // localhost or 127.0.0.1
    tregex localhost_regex = tregex(
                    _T("^([\\w]+=)?([a-z]+:\\/\\/)?(?:(?:localhost)|(?:127\\.0\\.0\\.1))(:[0-9]+)$"), 
                    regex::ECMAScript | regex::icase);
    // IPv4 (Note: very rough, but probably good enough)
    tregex ipv4_regex = tregex(
                    _T("^([\\w]+=)?([a-z]+:\\/\\/)?(?:\\d+\\.\\d+\\.\\d+\\.\\d+)(:[0-9]+)$"), 
                    regex::ECMAScript | regex::icase);
    // IPv6 (Note: also very rough but probably good enough)
    tregex ipv6_regex = tregex(
                    _T("^([\\w]+=)?([a-z]+:\\/\\/)?(?:\\[[a-fA-F0-9:]+\\])(:[0-9]+)$"), 
                    regex::ECMAScript | regex::icase);
    // not-fully-qualified domain name
    tregex nonfqdn_regex = tregex(
                    _T("^([\\w]+=)?([a-z]+:\\/\\/)?(?:[\\w\\-]+)(:[0-9]+)$"), 
                    regex::ECMAScript | regex::icase);
    // fully qualified domain name
    tregex fqdn_regex = tregex(
                    _T("^([\\w]+=)?([a-z]+:\\/\\/)?(?:[\\w\\-\\.]+)(:[0-9]+)$"), 
                    regex::ECMAScript | regex::icase);

    for (vector<ConnectionProxy>::iterator it = rawOriginalProxyInfo.begin();
         it != rawOriginalProxyInfo.end();
         it++)
    {
        ConnectionProxy info;

        // We can't usefully redact this value. Maybe it shouldn't be included?
        if (it->name.empty())
        {
            info.name = _T("");
        }
        else
        {
            info.name = _T("[REDACTED]");
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
                ss << regex_replace(*elem, localhost_regex, tstring(_T("$1[LOCALHOST]$2$3")));
            }
            else if (regex_match(*elem, ipv4_regex))
            {
                ss << regex_replace(*elem, ipv4_regex, tstring(_T("$1[IPV4]$2$3")));
            }
            else if (regex_match(*elem, ipv6_regex))
            {
                ss << regex_replace(*elem, ipv6_regex, tstring(_T("$1[IPV6]$2$3")));
            }
            else if (regex_match(*elem, nonfqdn_regex))
            {
                ss << regex_replace(*elem, nonfqdn_regex, tstring(_T("$1[NONFQDN]$2$3")));
            }
            else if (regex_match(*elem, fqdn_regex))
            {
                ss << regex_replace(*elem, fqdn_regex, tstring(_T("$1[FQDN]$2$3")));
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

        info.flagsString = ss.str();
        if (info.flagsString.length() > 0)
        {
            // Strip the trailing "|"
            info.flagsString.resize(info.flagsString.size()-1);
        }

        o_originalProxyInfo.push_back(info);
    }
}


/**
We write the default system proxy info to the registry at every app start.
When Psiphon connects, we also write that proxy info to the registry. When
Psiphon disconnects/exists, we remove the Psiphon info.
If Psiphon crashes while running, its proxy info will still be in the registry,
and if this is detected we'll restore the default system proxy info.
*/
void DoStartupSystemProxyWork()
{
    vector<ConnectionProxy> nativeProxyInfo, psiphonProxyInfo;
    ReadRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_NATIVE_PROXY_INFO, nativeProxyInfo);
    ReadRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_PSIPHON_PROXY_INFO, psiphonProxyInfo);

    if (psiphonProxyInfo.size())
    {
        if (!SetCurrentSystemConnectionsProxy(nativeProxyInfo))
        {
            // If we could not restore the original System Proxy Settings, don't clear
            // PSIPHON_PROXY_INFO since it is used as a signal on subsequent runs to
            // restore the original System Proxy Settings.
            // Also, don't write the current System Proxy Settings to
            // NATIVE_PROXY_INFO since we don't know what state the system is in.
            my_print(NOT_SENSITIVE, false, _T("%s:%d: SetConnectionProxy: %d"), __TFUNCTION__, __LINE__, GetLastError());
            return;
        }

        ClearRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_PSIPHON_PROXY_INFO);
    }

    GetCurrentSystemConnectionsProxyInfo(nativeProxyInfo);
    WriteRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_NATIVE_PROXY_INFO, nativeProxyInfo);

    // In an older version of Psiphon, the system proxy settings may have been left configured to
    // 127.0.0.1:<port> where port could have been 8080-8090.
    // Detect this condition, and check if there is actually anything running on the configured
    // system https proxy port. If there is nothing responding, we assume this case and will
    // ignore (and ultimately reset) the system proxy settings.
    DecomposedProxyConfig decomposedNativeDefaultProxyConfig;
    GetNativeDefaultProxyInfo(decomposedNativeDefaultProxyConfig);
    if (decomposedNativeDefaultProxyConfig.httpsProxy == _T("127.0.0.1") &&
        8080 <= decomposedNativeDefaultProxyConfig.httpsProxyPort &&
        decomposedNativeDefaultProxyConfig.httpsProxyPort <= 8090)
    {
        StopInfo stopInfo;
        if (ERROR_SUCCESS != WaitForConnectability(decomposedNativeDefaultProxyConfig.httpsProxyPort, 100, 0, stopInfo))
        {
            // There is nothing responding on the system https proxy port
            GetCurrentSystemConnectionsProxyInfo(nativeProxyInfo);
            for (vector<ConnectionProxy>::iterator ii = nativeProxyInfo.begin();
                ii != nativeProxyInfo.end();
                ++ii)
            {
                // These are the new proxy settings we want to use
                ii->flags = 0;
                ii->proxy = tstring();
                ii->bypass = tstring();
            }
            WriteRegistryProxyInfo(LOCAL_SETTINGS_REGISTRY_VALUE_NATIVE_PROXY_INFO, nativeProxyInfo);
        }
    }
}


void ClearRegistryProxyInfo(const char* regKey)
{
    WriteRegistryProxyInfo(regKey, vector<ConnectionProxy>());
}

void ReadRegistryProxyInfo(const char* regKey, vector<ConnectionProxy>& o_proxyInfo)
{
    o_proxyInfo.clear();

    string proxyJsonString;
    if (!ReadRegistryStringValue(regKey, proxyJsonString)
        || proxyJsonString.empty())
    {
        // No remnant proxy info, so it was cleaned up last time
        return;
    }

    Json::Value proxyJson;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(proxyJsonString, proxyJson);
    if (!parsingSuccessful)
    {
        string fail = reader.getFormattedErrorMessages();

        my_print(NOT_SENSITIVE, false, _T("%s:%d - JSON parse error: %S"), __TFUNCTION__, __LINE__, fail.c_str());
        ClearRegistryProxyInfo(regKey);
        return;
    }

    try
    {
        Json::Value proxiesJson = proxyJson["proxies"];
        for (Json::Value::ArrayIndex i = 0; i < proxiesJson.size(); i++)
        {
            ConnectionProxy proxy;
            proxy.name = UTF8ToWString(proxiesJson[i].get("name", "").asString());
            proxy.flags = proxiesJson[i].get("flags", 0).asUInt();
            proxy.proxy = UTF8ToWString(proxiesJson[i].get("proxy", "").asString());
            proxy.bypass = UTF8ToWString(proxiesJson[i].get("bypass", "").asString());
            o_proxyInfo.push_back(proxy);
        }
    }
    catch (exception& e)
    {
        o_proxyInfo.clear();
        my_print(NOT_SENSITIVE, false, _T("%s:%d: JSON parse exception: %S"), __TFUNCTION__, __LINE__, e.what());
        ClearRegistryProxyInfo(regKey);
        return;
    }
}

void WriteRegistryProxyInfo(const char* regKey, const vector<ConnectionProxy>& proxyInfo)
{
    Json::Value json;
    Json::Value proxies(Json::arrayValue);
    
    for (vector<ConnectionProxy>::const_iterator ii = proxyInfo.begin();
         ii != proxyInfo.end();
         ++ii)
    {
        Json::Value entry;
        entry["name"] = WStringToUTF8(ii->name);
        entry["flags"] = Json::UInt(ii->flags);
        entry["proxy"] = WStringToUTF8(ii->proxy);
        entry["bypass"] = WStringToUTF8(ii->bypass);
        proxies.append(entry);
    }

    json["proxies"] = proxies;

    ostringstream jsonStringStream; 
    Json::FastWriter jsonWriter;
    jsonStringStream << jsonWriter.write(json); 
    string jsonString = jsonStringStream.str();

    RegistryFailureReason registryFailureReason;
    if (!WriteRegistryStringValue(regKey, 
                                  jsonString, 
                                  registryFailureReason))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d: WriteRegistryStringValue error: %d, %d"), __TFUNCTION__, __LINE__, registryFailureReason, GetLastError());
    }
}


void DecomposeDefaultProxyInfo(const ConnectionProxy& proxyInfo, DecomposedProxyConfig& o_proxyConfig)
{
    o_proxyConfig.clear();

    if (!(proxyInfo.flags & PROXY_TYPE_PROXY))
    {
        return;
    }

    tstring proxy_str = proxyInfo.proxy;

    std::size_t colon_pos;
    std::size_t equal_pos;

    colon_pos = proxy_str.find(':');

    /*
    case 1: no ':' in the proxy_str
    ""
    proxy host and port are not set
    */
    if (tstring::npos == colon_pos)
    {
        return;
    }

    /*
    case 2: '=' protocol identifier not found in the proxy_str
    "host:port"
    same proxy used for all protocols
    */
    equal_pos = proxy_str.find('=');
    if(tstring::npos == equal_pos)
    {
        //store it
        o_proxyConfig.httpProxy = proxy_str;
        o_proxyConfig.httpsProxy = proxy_str;
        o_proxyConfig.socksProxy = proxy_str;
    }

    /*
    case 3: '=' protocol identifier found in the proxy_str,
    "http=host:port;https=host:port;ftp=host:port;socks=host:port"
    loop through proxy types, pick  https or socks in that order
    */

    //split by protocol
    std::size_t prev = 0, pos;
    tstring protocol, proxy;
    while ((pos = proxy_str.find('=', prev)) != tstring::npos)
    {
        if (pos > prev)
        {
            protocol = (proxy_str.substr(prev, pos-prev));
        }
        prev = pos+1;

        pos = proxy_str.find(';', prev);

        if(pos == tstring::npos)
        {
            proxy = (proxy_str.substr(prev, tstring::npos));
        }
        else if(pos >= prev)
        {
            proxy =  (proxy_str.substr(prev, pos-prev));
            prev = pos+1;
        }

        if (protocol == _T("http"))
        {
            o_proxyConfig.httpProxy = proxy;
        }
        else if (protocol == _T("https"))
        {
            o_proxyConfig.httpsProxy = proxy;
        }
        else if (protocol == _T("socks"))
        {
            o_proxyConfig.socksProxy = proxy;
        }
    }

    /*
     * At this point the proxy fields are actually host:port. Decompose.
     */

    proxy_str = o_proxyConfig.httpProxy;
    colon_pos = proxy_str.find(':');
    if(colon_pos != tstring::npos)
    {
        tstring port_str = proxy_str.substr(colon_pos+1);
        o_proxyConfig.httpProxyPort = _wtoi(port_str.c_str());
        o_proxyConfig.httpProxy = proxy_str.substr(0,colon_pos);
    }
    else
    {
        o_proxyConfig.httpProxy.clear();
    }
    
    proxy_str = o_proxyConfig.httpsProxy;
    colon_pos = proxy_str.find(':');
    if(colon_pos != tstring::npos)
    {
        tstring port_str = proxy_str.substr(colon_pos+1);
        o_proxyConfig.httpsProxyPort = _wtoi(port_str.c_str());
        o_proxyConfig.httpsProxy = proxy_str.substr(0,colon_pos);
    }
    else
    {
        o_proxyConfig.httpsProxy.clear();
    }
    
    proxy_str = o_proxyConfig.socksProxy;
    colon_pos = proxy_str.find(':');
    if(colon_pos != tstring::npos)
    {
        tstring port_str = proxy_str.substr(colon_pos+1);
        o_proxyConfig.socksProxyPort = _wtoi(port_str.c_str());
        o_proxyConfig.socksProxy = proxy_str.substr(0,colon_pos);
    }
    else
    {
        o_proxyConfig.socksProxy.clear();
    }
}

