/*
 * Copyright (c) 2015, Psiphon Inc.
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
#include "resource.h"
#include "psiclient.h"
#include "usersettings.h"
#include "utilities.h"
#include "htmldlg.h"
#include "coretransport.h"
#include "vpntransport.h"


#define NULL_PORT                       0
#define MAX_PORT                        0xFFFF

#define SPLIT_TUNNEL_NAME               "SplitTunnel"
#define SPLIT_TUNNEL_DEFAULT            FALSE

#define TRANSPORT_NAME                  "Transport"
// TODO: Don't hardcode transport names? Or get rid of transport registry (since the dynamic-ness is gone anyway).
#define TRANSPORT_DEFAULT               CORE_TRANSPORT_DISPLAY_NAME
#define TRANSPORT_VPN                   VPN_TRANSPORT_DISPLAY_NAME

#define HTTP_PROXY_PORT_NAME            "UserLocalHTTPProxyPort"
#define HTTP_PROXY_PORT_DEFAULT         NULL_PORT
#define SOCKS_PROXY_PORT_NAME           "UserLocalSOCKSProxyPort"
#define SOCKS_PROXY_PORT_DEFAULT        NULL_PORT

#define EGRESS_REGION_NAME              "EgressRegion"
#define EGRESS_REGION_DEFAULT           ""

#define SKIP_BROWSER_NAME               "UserSkipBrowser"
#define SKIP_BROWSER_DEFAULT            FALSE

#define SKIP_PROXY_SETTINGS_NAME        "UserSkipProxySettings"
#define SKIP_PROXY_SETTINGS_DEFAULT     FALSE

#define SKIP_UPSTREAM_PROXY_NAME        "UserSkipSSHParentProxySettings"
#define SKIP_UPSTREAM_PROXY_DEFAULT     FALSE

#define UPSTREAM_PROXY_TYPE_NAME        "UserSSHParentProxyType"
#define UPSTREAM_PROXY_TYPE_DEFAULT     "https"

#define UPSTREAM_PROXY_HOSTNAME_NAME    "UserSSHParentProxyHostname"
#define UPSTREAM_PROXY_HOSTNAME_DEFAULT ""

#define UPSTREAM_PROXY_PORT_NAME        "UserSSHParentProxyPort"
#define UPSTREAM_PROXY_PORT_DEFAULT     NULL_PORT

static HANDLE g_registryMutex = CreateMutex(NULL, FALSE, 0);

int GetUserSettingDword(const string& settingName, int defaultValue)
{
    AutoMUTEX lock(g_registryMutex);

    DWORD value = 0;

    if (!ReadRegistryDwordValue(settingName, value))
    {
        // Write out the setting with a default value so that it's there
        // for users to see and use, if they want to set it.
        value = defaultValue;
        WriteRegistryDwordValue(settingName, value);
    }

    return value;
}

string GetUserSettingString(const string& settingName, string defaultValue)
{
    AutoMUTEX lock(g_registryMutex);

    string value;

    if (!ReadRegistryStringValue(settingName.c_str(), value))
    {
        // Write out the setting with a default value so that it's there
        // for users to see and use, if they want to set it.
        value = defaultValue;
        RegistryFailureReason reason = REGISTRY_FAILURE_NO_REASON;
        WriteRegistryStringValue(settingName, value, reason);
    }

    return value;
}

wstring GetUserSettingString(const string& settingName, wstring defaultValue)
{
    AutoMUTEX lock(g_registryMutex);

    wstring value;

    if (!ReadRegistryStringValue(settingName.c_str(), value))
    {
        // Write out the setting with a default value so that it's there
        // for users to see and use, if they want to set it.
        value = defaultValue;
        RegistryFailureReason reason = REGISTRY_FAILURE_NO_REASON;
        WriteRegistryStringValue(settingName, value, reason);
    }

    return value;
}

void Settings::Initialize()
{
    // Read - and consequently write out default values for - all settings
    (void)Settings::SplitTunnel();
    (void)Settings::Transport();
    (void)Settings::LocalHttpProxyPort();
    (void)Settings::LocalSocksProxyPort();
}

bool Settings::Show(HINSTANCE hInst, HWND hParentWnd)
{
    Json::Value config;
    config["SplitTunnel"] = Settings::SplitTunnel();
    config["VPN"] = (Settings::Transport() == TRANSPORT_VPN);
    config["LocalHttpProxyPort"] = Settings::LocalHttpProxyPort();
    config["LocalSocksProxyPort"] = Settings::LocalSocksProxyPort();
    config["SkipUpstreamProxy"] = Settings::SkipUpstreamProxy();
    config["UpstreamProxyHostname"] = Settings::UpstreamProxyHostname();
    config["UpstreamProxyPort"] = Settings::UpstreamProxyPort();
    config["EgressRegion"] = Settings::EgressRegion();
    config["defaults"] = Json::Value();
    config["defaults"]["SplitTunnel"] = SPLIT_TUNNEL_DEFAULT;
    config["defaults"]["VPN"] = FALSE;
    config["defaults"]["LocalHttpProxyPort"] = NULL_PORT;
    config["defaults"]["LocalSocksProxyPort"] = NULL_PORT;
    config["defaults"]["SkipUpstreamProxy"] = SKIP_UPSTREAM_PROXY_DEFAULT;
    config["defaults"]["UpstreamProxyHostname"] = UPSTREAM_PROXY_HOSTNAME_DEFAULT;
    config["defaults"]["UpstreamProxyPort"] = NULL_PORT;
    config["defaults"]["EgressRegion"] = EGRESS_REGION_DEFAULT;

    stringstream configDataStream;
    Json::FastWriter jsonWriter;
    configDataStream << jsonWriter.write(config);

    tstring result;
    if (ShowHTMLDlg(
        hParentWnd,
        _T("SETTINGS_HTML_RESOURCE"),
        GetLocaleName().c_str(),
        NarrowToTString(configDataStream.str()).c_str(),
        result) != 1)
    {
        // error or user cancelled
        return false;
    }

    Json::Value json;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(WStringToUTF8(result.c_str()), json);
    if (!parsingSuccessful)
    {
        my_print(NOT_SENSITIVE, false, _T("Failed to save settings!"));
        return false;
    }

    bool settingsChanged = false;

    try
    {
        AutoMUTEX lock(g_registryMutex);

        // Note: We're not purposely not bothering to check registry write return values.

        RegistryFailureReason failReason;

        BOOL splitTunnel = json.get("SplitTunnel", 0).asUInt();
        settingsChanged = settingsChanged || !!splitTunnel != Settings::SplitTunnel();
        WriteRegistryDwordValue(SPLIT_TUNNEL_NAME, splitTunnel);

        wstring transport = json.get("VPN", 0).asUInt() ? TRANSPORT_VPN : TRANSPORT_DEFAULT;
        settingsChanged = settingsChanged || transport != Settings::Transport();
        WriteRegistryStringValue(
            TRANSPORT_NAME,
            transport,
            failReason);

        DWORD httpPort = json.get("LocalHttpProxyPort", 0).asUInt();
        settingsChanged = settingsChanged || httpPort != Settings::LocalHttpProxyPort();
        WriteRegistryDwordValue(HTTP_PROXY_PORT_NAME, httpPort);

        DWORD socksPort = json.get("LocalSocksProxyPort", 0).asUInt();
        settingsChanged = settingsChanged || socksPort != Settings::LocalSocksProxyPort();
        WriteRegistryDwordValue(SOCKS_PROXY_PORT_NAME, socksPort);

        string upstreamProxyHostname = json.get("UpstreamProxyHostname", "").asString();
        settingsChanged = settingsChanged || upstreamProxyHostname != Settings::UpstreamProxyHostname();
        WriteRegistryStringValue(
            UPSTREAM_PROXY_HOSTNAME_NAME,
            upstreamProxyHostname,
            failReason);

        DWORD upstreamProxyPort = json.get("UpstreamProxyPort", 0).asUInt();
        settingsChanged = settingsChanged || upstreamProxyPort != Settings::UpstreamProxyPort();
        WriteRegistryDwordValue(UPSTREAM_PROXY_PORT_NAME, upstreamProxyPort);

        BOOL skipUpstreamProxy = json.get("SkipUpstreamProxy", 0).asUInt();
        settingsChanged = settingsChanged || !!skipUpstreamProxy != Settings::SkipUpstreamProxy();
        WriteRegistryDwordValue(SKIP_UPSTREAM_PROXY_NAME, skipUpstreamProxy);

        string egressRegion = json.get("EgressRegion", "").asString();
        settingsChanged = settingsChanged || egressRegion != Settings::EgressRegion();
        WriteRegistryStringValue(
            EGRESS_REGION_NAME,
            egressRegion,
            failReason);
    }
    catch (exception& e)
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d: JSON parse exception: %S"), __TFUNCTION__, __LINE__, e.what());
    }

    return settingsChanged;
}

bool Settings::SplitTunnel()
{
    // Not yet supported!
    //return !!GetUserSettingDword(SPLIT_TUNNEL_NAME, SPLIT_TUNNEL_DEFAULT);
    return false;
}

tstring Settings::Transport()
{
    tstring transport = GetUserSettingString(TRANSPORT_NAME, TRANSPORT_DEFAULT);
    if (transport != TRANSPORT_VPN)
    {
        transport = TRANSPORT_DEFAULT;
    }
    return transport;
}

unsigned int Settings::LocalHttpProxyPort()
{
    DWORD port = GetUserSettingDword(HTTP_PROXY_PORT_NAME, HTTP_PROXY_PORT_DEFAULT);
    if (port > MAX_PORT)
    {
        port = HTTP_PROXY_PORT_DEFAULT;
    }
    return (unsigned int)port;
}

unsigned int Settings::LocalSocksProxyPort()
{
    DWORD port = GetUserSettingDword(SOCKS_PROXY_PORT_NAME, SOCKS_PROXY_PORT_DEFAULT);
    if (port > MAX_PORT)
    {
        port = SOCKS_PROXY_PORT_DEFAULT;
    }
    return (unsigned int)port;
}

string Settings::UpstreamProxyType()
{
    // We only support one type, but we'll call this to create the registry entry
    (void)GetUserSettingString(UPSTREAM_PROXY_TYPE_NAME, UPSTREAM_PROXY_TYPE_DEFAULT);
    return UPSTREAM_PROXY_TYPE_DEFAULT;
}

string Settings::UpstreamProxyHostname()
{
    return GetUserSettingString(UPSTREAM_PROXY_HOSTNAME_NAME, UPSTREAM_PROXY_HOSTNAME_DEFAULT);
}

unsigned int Settings::UpstreamProxyPort()
{
    DWORD port = GetUserSettingDword(UPSTREAM_PROXY_PORT_NAME, UPSTREAM_PROXY_PORT_DEFAULT);
    if (port > MAX_PORT)
    {
        port = UPSTREAM_PROXY_PORT_DEFAULT;
    }
    return (unsigned int)port;
}

bool Settings::SkipUpstreamProxy()
{
    return !!GetUserSettingDword(SKIP_UPSTREAM_PROXY_NAME, SKIP_UPSTREAM_PROXY_DEFAULT);
}

string Settings::EgressRegion()
{
    return GetUserSettingString(EGRESS_REGION_NAME, EGRESS_REGION_DEFAULT);
}

/*
Settings that are not exposed in the UI.
*/

bool Settings::SkipBrowser()
{
    return !!GetUserSettingDword(SKIP_BROWSER_NAME, SKIP_BROWSER_DEFAULT);
}
    
bool Settings::SkipProxySettings()
{
    return !!GetUserSettingDword(SKIP_PROXY_SETTINGS_NAME, SKIP_PROXY_SETTINGS_DEFAULT);
}
