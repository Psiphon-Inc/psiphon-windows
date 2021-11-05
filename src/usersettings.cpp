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
#include "logging.h"
#include "psiclient.h"
#include "usersettings.h"
#include "utilities.h"
#include "coretransport.h"
#include "vpntransport.h"


#define NULL_PORT                       0
#define MAX_PORT                        0xFFFF

#define SPLIT_TUNNEL_NAME               "SplitTunnel"
#define SPLIT_TUNNEL_DEFAULT            FALSE

#define SPLIT_TUNNEL_CHINESE_SITES_NAME "SplitTunnelChineseSites"
#define SPLIT_TUNNEL_CHINESE_SITES_DEFAULT  FALSE

#define DISABLE_TIMEOUTS_NAME           "DisableTimeouts"
#define DISABLE_TIMEOUTS_DEFAULT        FALSE

#define TRANSPORT_NAME                  "Transport"
// TODO: Don't hardcode transport names? Or get rid of transport registry (since the dynamic-ness is gone anyway).
#define TRANSPORT_DEFAULT               CORE_TRANSPORT_PROTOCOL_NAME
#define TRANSPORT_VPN                   VPN_TRANSPORT_PROTOCOL_NAME

#define HTTP_PROXY_PORT_NAME            "LocalHTTPProxyPort"
#define HTTP_PROXY_PORT_DEFAULT         NULL_PORT
#define SOCKS_PROXY_PORT_NAME           "LocalSOCKSProxyPort"
#define SOCKS_PROXY_PORT_DEFAULT        NULL_PORT

#define EXPOSE_LOCAL_PROXIES_TO_LAN_NAME    "ExposeLocalProxiesToLAN"
#define EXPOSE_LOCAL_PROXIES_TO_LAN_DEFAULT FALSE

#define EGRESS_REGION_NAME              "EgressRegion"
#define EGRESS_REGION_DEFAULT           ""

#define SKIP_BROWSER_DEFAULT            FALSE

#define SKIP_PROXY_SETTINGS_NAME        "SkipProxySettings"
#define SKIP_PROXY_SETTINGS_DEFAULT     FALSE

#define SKIP_AUTO_CONNECT_NAME          "SkipAutoConnect"
#define SKIP_AUTO_CONNECT_DEFAULT       FALSE

#define SKIP_UPSTREAM_PROXY_NAME        "SSHParentProxySkip"
#define SKIP_UPSTREAM_PROXY_DEFAULT     FALSE

#define UPSTREAM_PROXY_TYPE_NAME        "SSHParentProxyType"
#define UPSTREAM_PROXY_TYPE_DEFAULT     "https"


#define UPSTREAM_PROXY_HOSTNAME_NAME    "SSHParentProxyHost"
#define UPSTREAM_PROXY_HOSTNAME_DEFAULT ""
// We used to store the hostname formatted with authinfo@hostname, but we're migrating away from that
#define UPSTREAM_PROXY_HOSTNAME_NAME_DEFUNCT    "SSHParentProxyHostname"
#define UPSTREAM_PROXY_HOSTNAME_DEFAULT_DEFUNCT ""

#define UPSTREAM_PROXY_PORT_NAME        "SSHParentProxyPort"
#define UPSTREAM_PROXY_PORT_DEFAULT     NULL_PORT

#define UPSTREAM_PROXY_USERNAME_NAME    "SSHParentProxyUsername"
#define UPSTREAM_PROXY_USERNAME_DEFAULT ""

#define UPSTREAM_PROXY_PASSWORD_NAME    "SSHParentProxyPassword"
#define UPSTREAM_PROXY_PASSWORD_DEFAULT ""

#define UPSTREAM_PROXY_DOMAIN_NAME      "SSHParentProxyDomain"
#define UPSTREAM_PROXY_DOMAIN_DEFAULT   ""

#define SYSTRAY_MINIMIZE_NAME           "SystrayMinimize"
#define SYSTRAY_MINIMIZE_DEFAULT        FALSE

#define DISABLE_DISALLOWED_TRAFFIC_ALERT_NAME           "DisableDisallowedTrafficAlert"
#define DISABLE_DISALLOWED_TRAFFIC_ALERT_DEFAULT        FALSE

#define COOKIES_NAME                    "UICookies"
#define COOKIES_DEFAULT                 ""

#define WINDOW_PLACEMENT_NAME           "UIWindowPlacement"
#define WINDOW_PLACEMENT_DEFAULT        ""


static HANDLE g_registryMutex = CreateMutex(NULL, FALSE, 0);

int GetSettingDword(const string& settingName, int defaultValue, bool writeDefault=false)
{
    AutoMUTEX lock(g_registryMutex);

    DWORD value = 0;

    if (!ReadRegistryDwordValue(settingName, value))
    {
        value = defaultValue;

        if (writeDefault)
        {
            WriteRegistryDwordValue(settingName, value);
        }
    }

    return value;
}

string GetSettingString(const string& settingName, string defaultValue, bool writeDefault=false)
{
    AutoMUTEX lock(g_registryMutex);

    string value;

    if (!ReadRegistryStringValue(settingName.c_str(), value))
    {
        value = defaultValue;

        if (writeDefault)
        {
            RegistryFailureReason reason = REGISTRY_FAILURE_NO_REASON;
            WriteRegistryStringValue(settingName, value, reason);
        }
    }

    return value;
}

wstring GetSettingString(const string& settingName, wstring defaultValue, bool writeDefault=false)
{
    AutoMUTEX lock(g_registryMutex);

    wstring value;

    if (!ReadRegistryStringValue(settingName.c_str(), value))
    {
        value = defaultValue;

        if (writeDefault)
        {
            RegistryFailureReason reason = REGISTRY_FAILURE_NO_REASON;
            WriteRegistryStringValue(settingName, value, reason);
        }
    }

    return value;
}

bool DoesSettingExist(const string& settingName)
{
    AutoMUTEX lock(g_registryMutex);

    return DoesRegistryValueExist(settingName);
}

void Settings::Initialize()
{
    // Write out the default values for our non-exposed (registry-only) settings.
    // This is to help users find and modify them.
    (void)GetSettingDword(SKIP_PROXY_SETTINGS_NAME, SKIP_PROXY_SETTINGS_DEFAULT, true);
    (void)GetSettingDword(SKIP_AUTO_CONNECT_NAME, SKIP_AUTO_CONNECT_DEFAULT, true);
}

void Settings::ToJson(Json::Value& o_json)
{
      o_json.clear();
      o_json["defaults"] = Json::Value();

      o_json["SplitTunnel"] = Settings::SplitTunnel() ? TRUE : FALSE;
      o_json["defaults"]["SplitTunnel"] = SPLIT_TUNNEL_DEFAULT;

      o_json["SplitTunnelChineseSites"] = Settings::SplitTunnelChineseSites() ? TRUE : FALSE;
      o_json["defaults"]["SplitTunnelChineseSites"] = SPLIT_TUNNEL_CHINESE_SITES_DEFAULT;

      o_json["DisableTimeouts"] = Settings::DisableTimeouts() ? TRUE : FALSE;
      o_json["defaults"]["DisableTimeouts"] = DISABLE_TIMEOUTS_DEFAULT;

      o_json["VPN"] = (Settings::Transport() == TRANSPORT_VPN) ? TRUE : FALSE;
      o_json["defaults"]["VPN"] = FALSE;

      o_json["LocalHttpProxyPort"] = Settings::LocalHttpProxyPort();
      o_json["defaults"]["LocalHttpProxyPort"] = HTTP_PROXY_PORT_DEFAULT;
      o_json["LocalSocksProxyPort"] = Settings::LocalSocksProxyPort();
      o_json["defaults"]["LocalSocksProxyPort"] = SOCKS_PROXY_PORT_DEFAULT;

      o_json["ExposeLocalProxiesToLAN"] = Settings::ExposeLocalProxiesToLAN() ? TRUE : FALSE;
      o_json["defaults"]["ExposeLocalProxiesToLAN"] = EXPOSE_LOCAL_PROXIES_TO_LAN_DEFAULT;

      o_json["SkipUpstreamProxy"] = Settings::SkipUpstreamProxy() ? TRUE : FALSE;
      o_json["defaults"]["SkipUpstreamProxy"] = SKIP_UPSTREAM_PROXY_DEFAULT;
      o_json["UpstreamProxyHostname"] = Settings::UpstreamProxyHostname();
      o_json["defaults"]["UpstreamProxyHostname"] = UPSTREAM_PROXY_HOSTNAME_DEFAULT;
      o_json["UpstreamProxyPort"] = Settings::UpstreamProxyPort();
      o_json["defaults"]["UpstreamProxyPort"] = UPSTREAM_PROXY_PORT_DEFAULT;
      o_json["UpstreamProxyUsername"] = Settings::UpstreamProxyUsername();
      o_json["defaults"]["UpstreamProxyUsername"] = UPSTREAM_PROXY_USERNAME_DEFAULT;
      o_json["UpstreamProxyPassword"] = Settings::UpstreamProxyPassword();
      o_json["defaults"]["UpstreamProxyPassword"] = UPSTREAM_PROXY_PASSWORD_DEFAULT;
      o_json["UpstreamProxyDomain"] = Settings::UpstreamProxyDomain();
      o_json["defaults"]["UpstreamProxyDomain"] = UPSTREAM_PROXY_DOMAIN_DEFAULT;

      o_json["EgressRegion"] = Settings::EgressRegion();
      o_json["defaults"]["EgressRegion"] = EGRESS_REGION_DEFAULT;

      o_json["SystrayMinimize"] = Settings::SystrayMinimize() ? TRUE : FALSE;
      o_json["defaults"]["SystrayMinimize"] = SYSTRAY_MINIMIZE_DEFAULT;

      o_json["DisableDisallowedTrafficAlert"] = Settings::DisableDisallowedTrafficAlert() ? TRUE : FALSE;
      o_json["defaults"]["DisableDisallowedTrafficAlert"] = DISABLE_DISALLOWED_TRAFFIC_ALERT_DEFAULT;
}

// FromJson updates the stores settings from an object stored in JSON format.
bool Settings::FromJson(
    const string& utf8JSON,
    bool& o_reconnectRequired)
{
    o_reconnectRequired = false;

    Json::Value json;
    Json::Reader reader;

    bool parsingSuccessful = reader.parse(utf8JSON, json);
    if (!parsingSuccessful)
    {
        my_print(NOT_SENSITIVE, false, _T("Failed to save settings!"));
        return false;
    }

    bool reconnectRequiredValueChanged = false;

    try
    {
        AutoMUTEX lock(g_registryMutex);

        // Note: We're purposely not bothering to check registry write return values.

        RegistryFailureReason failReason;

        BOOL splitTunnel = json.get("SplitTunnel", SPLIT_TUNNEL_DEFAULT).asUInt();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || !!splitTunnel != Settings::SplitTunnel();
        WriteRegistryDwordValue(SPLIT_TUNNEL_NAME, splitTunnel);

        BOOL splitTunnelChineseSites = json.get("SplitTunnelChineseSites", SPLIT_TUNNEL_CHINESE_SITES_DEFAULT).asUInt();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || !!splitTunnelChineseSites != Settings::SplitTunnelChineseSites();
        WriteRegistryDwordValue(SPLIT_TUNNEL_CHINESE_SITES_NAME, splitTunnelChineseSites);

        BOOL disableTimeouts = json.get("DisableTimeouts", DISABLE_TIMEOUTS_DEFAULT).asUInt();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || !!disableTimeouts != Settings::DisableTimeouts();
        WriteRegistryDwordValue(DISABLE_TIMEOUTS_NAME, disableTimeouts);

        wstring transport = json.get("VPN", TRANSPORT_DEFAULT).asUInt() ? TRANSPORT_VPN : TRANSPORT_DEFAULT;
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || transport != Settings::Transport();
        WriteRegistryStringValue(
            TRANSPORT_NAME,
            transport,
            failReason);

        DWORD httpPort = json.get("LocalHttpProxyPort", HTTP_PROXY_PORT_DEFAULT).asUInt();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || httpPort != Settings::LocalHttpProxyPort();
        WriteRegistryDwordValue(HTTP_PROXY_PORT_NAME, httpPort);

        DWORD socksPort = json.get("LocalSocksProxyPort", SOCKS_PROXY_PORT_DEFAULT).asUInt();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || socksPort != Settings::LocalSocksProxyPort();
        WriteRegistryDwordValue(SOCKS_PROXY_PORT_NAME, socksPort);

        BOOL exposeLocalProxiesToLAN = json.get("ExposeLocalProxiesToLAN", EXPOSE_LOCAL_PROXIES_TO_LAN_DEFAULT).asUInt();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || !!exposeLocalProxiesToLAN != Settings::ExposeLocalProxiesToLAN();
        WriteRegistryDwordValue(EXPOSE_LOCAL_PROXIES_TO_LAN_NAME, exposeLocalProxiesToLAN);

        string upstreamProxyUsername = json.get("UpstreamProxyUsername", UPSTREAM_PROXY_USERNAME_DEFAULT).asString();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || upstreamProxyUsername != Settings::UpstreamProxyUsername();
        WriteRegistryStringValue(
            UPSTREAM_PROXY_USERNAME_NAME,
            upstreamProxyUsername,
            failReason);

        string upstreamProxyPassword = json.get("UpstreamProxyPassword", UPSTREAM_PROXY_PASSWORD_DEFAULT).asString();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || upstreamProxyPassword != Settings::UpstreamProxyPassword();
        WriteRegistryStringValue(
            UPSTREAM_PROXY_PASSWORD_NAME,
            upstreamProxyPassword,
            failReason);

        string upstreamProxyDomain = json.get("UpstreamProxyDomain", UPSTREAM_PROXY_DOMAIN_DEFAULT).asString();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || upstreamProxyDomain != Settings::UpstreamProxyDomain();
        WriteRegistryStringValue(
            UPSTREAM_PROXY_DOMAIN_NAME,
            upstreamProxyDomain,
            failReason);

        string upstreamProxyHostname = json.get("UpstreamProxyHostname", UPSTREAM_PROXY_HOSTNAME_DEFAULT).asString();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || upstreamProxyHostname != Settings::UpstreamProxyHostname();
        WriteRegistryStringValue(
            UPSTREAM_PROXY_HOSTNAME_NAME,
            upstreamProxyHostname,
            failReason);

        DWORD upstreamProxyPort = json.get("UpstreamProxyPort", UPSTREAM_PROXY_PORT_DEFAULT).asUInt();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || upstreamProxyPort != Settings::UpstreamProxyPort();
        WriteRegistryDwordValue(UPSTREAM_PROXY_PORT_NAME, upstreamProxyPort);

        BOOL skipUpstreamProxy = json.get("SkipUpstreamProxy", SKIP_UPSTREAM_PROXY_DEFAULT).asUInt();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || !!skipUpstreamProxy != Settings::SkipUpstreamProxy();
        WriteRegistryDwordValue(SKIP_UPSTREAM_PROXY_NAME, skipUpstreamProxy);

        string egressRegion = json.get("EgressRegion", EGRESS_REGION_DEFAULT).asString();
        reconnectRequiredValueChanged = reconnectRequiredValueChanged || egressRegion != Settings::EgressRegion();
        WriteRegistryStringValue(
            EGRESS_REGION_NAME,
            egressRegion,
            failReason);

        BOOL systrayMinimize = json.get("SystrayMinimize", SYSTRAY_MINIMIZE_DEFAULT).asUInt();
        // Does not require reconnect to apply change.
        WriteRegistryDwordValue(SYSTRAY_MINIMIZE_NAME, systrayMinimize);

        BOOL disableDisallowedTrafficAlert = json.get("DisableDisallowedTrafficAlert", DISABLE_DISALLOWED_TRAFFIC_ALERT_DEFAULT).asUInt();
        // Does not require reconnect to apply change.
        WriteRegistryDwordValue(DISABLE_DISALLOWED_TRAFFIC_ALERT_NAME, disableDisallowedTrafficAlert);
    }
    catch (exception& e)
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d: JSON parse exception: %S"), __TFUNCTION__, __LINE__, e.what());
        return false;
    }

    o_reconnectRequired = reconnectRequiredValueChanged;

    return true;
}

bool Settings::SplitTunnel()
{
    return !!GetSettingDword(SPLIT_TUNNEL_NAME, SPLIT_TUNNEL_DEFAULT);
}

bool Settings::SplitTunnelChineseSites()
{
    return !!GetSettingDword(SPLIT_TUNNEL_CHINESE_SITES_NAME, SPLIT_TUNNEL_CHINESE_SITES_DEFAULT);
}

bool Settings::DisableTimeouts()
{
    return !!GetSettingDword(DISABLE_TIMEOUTS_NAME, DISABLE_TIMEOUTS_DEFAULT);
}

tstring Settings::Transport()
{
    tstring transport = GetSettingString(TRANSPORT_NAME, TRANSPORT_DEFAULT);
    if (transport != TRANSPORT_VPN)
    {
        transport = TRANSPORT_DEFAULT;
    }
    return transport;
}

unsigned int Settings::LocalHttpProxyPort()
{
    DWORD port = GetSettingDword(HTTP_PROXY_PORT_NAME, HTTP_PROXY_PORT_DEFAULT);
    if (port > MAX_PORT)
    {
        port = HTTP_PROXY_PORT_DEFAULT;
    }
    return (unsigned int)port;
}

unsigned int Settings::LocalSocksProxyPort()
{
    DWORD port = GetSettingDword(SOCKS_PROXY_PORT_NAME, SOCKS_PROXY_PORT_DEFAULT);
    if (port > MAX_PORT)
    {
        port = SOCKS_PROXY_PORT_DEFAULT;
    }
    return (unsigned int)port;
}

bool Settings::ExposeLocalProxiesToLAN()
{
    return !!GetSettingDword(EXPOSE_LOCAL_PROXIES_TO_LAN_NAME, EXPOSE_LOCAL_PROXIES_TO_LAN_DEFAULT);
}

string Settings::UpstreamProxyType()
{
    return UPSTREAM_PROXY_TYPE_DEFAULT;
}

string Settings::UpstreamProxyHostname()
{
    if (DoesSettingExist(UPSTREAM_PROXY_HOSTNAME_NAME))
    {
        return GetSettingString(UPSTREAM_PROXY_HOSTNAME_NAME, UPSTREAM_PROXY_HOSTNAME_DEFAULT);
    }

    // Check the defunct key, as we might have to migrate the old value
    string hostname;
    string defunctAuthHostname = GetSettingString(UPSTREAM_PROXY_HOSTNAME_NAME_DEFUNCT, UPSTREAM_PROXY_HOSTNAME_DEFAULT_DEFUNCT);
    int splitIndex = defunctAuthHostname.find_first_of("@");
    if (splitIndex != string::npos) {
        hostname = defunctAuthHostname.substr(splitIndex + 1, defunctAuthHostname.length() - 1);
    }

    // Attempt to write the extracted value to the new key, so we don't have to do this every time.
    RegistryFailureReason failureReason; // ignored, along with return value -- if this write fails we'll do it again next time
    (void)WriteRegistryStringValue(UPSTREAM_PROXY_HOSTNAME_NAME, hostname, failureReason);

    return hostname;
}

unsigned int Settings::UpstreamProxyPort()
{
    DWORD port = GetSettingDword(UPSTREAM_PROXY_PORT_NAME, UPSTREAM_PROXY_PORT_DEFAULT);
    if (port > MAX_PORT)
    {
        port = UPSTREAM_PROXY_PORT_DEFAULT;
    }
    return (unsigned int)port;
}

string Settings::UpstreamProxyUsername()
{
    if (DoesSettingExist(UPSTREAM_PROXY_USERNAME_NAME))
    {
        return GetSettingString(UPSTREAM_PROXY_USERNAME_NAME, UPSTREAM_PROXY_USERNAME_DEFAULT);
    }

    // Check the defunct key, as we might have to migrate the old value
    string upstreamProxyAuthenticatedHostname = GetSettingString(UPSTREAM_PROXY_HOSTNAME_NAME_DEFUNCT, UPSTREAM_PROXY_HOSTNAME_DEFAULT_DEFUNCT);

    string username;
    int splitIndex = upstreamProxyAuthenticatedHostname.find_first_of("@");
    if (splitIndex != string::npos) {
        string splitString = upstreamProxyAuthenticatedHostname.substr(0, splitIndex);

        splitIndex = splitString.find_first_of(":");
        username = splitString.substr(0, splitIndex);

        splitIndex = username.find_first_of("\\");
        if (splitIndex != string::npos) {
            username = username.substr(splitIndex + 1, username.length() - 1);
        }
    }

    // Attempt to write the extracted value to the new key, so we don't have to do this every time.
    RegistryFailureReason failureReason; // ignored, along with return value -- if this write fails we'll do it again next time
    (void)WriteRegistryStringValue(UPSTREAM_PROXY_USERNAME_NAME, username, failureReason);

    return username;
}

string Settings::UpstreamProxyPassword()
{
    if (DoesSettingExist(UPSTREAM_PROXY_PASSWORD_NAME))
    {
        return GetSettingString(UPSTREAM_PROXY_PASSWORD_NAME, UPSTREAM_PROXY_PASSWORD_DEFAULT);
    }

    // Check the defunct key, as we might have to migrate the old value
    string password;
    string upstreamProxyAuthenticatedHostname = GetSettingString(UPSTREAM_PROXY_HOSTNAME_NAME_DEFUNCT, UPSTREAM_PROXY_HOSTNAME_DEFAULT_DEFUNCT);

    int splitIndex = upstreamProxyAuthenticatedHostname.find_first_of("@");
    if (splitIndex != string::npos) {
        string splitString = upstreamProxyAuthenticatedHostname.substr(0, splitIndex);

        splitIndex = splitString.find_first_of(":");
        password = splitString.substr(splitIndex + 1, upstreamProxyAuthenticatedHostname.length() - 1);
    }

    // Attempt to write the extracted value to the new key, so we don't have to do this every time.
    RegistryFailureReason failureReason; // ignored, along with return value -- if this write fails we'll do it again next time
    (void)WriteRegistryStringValue(UPSTREAM_PROXY_PASSWORD_NAME, password, failureReason);

    return password;
}

string Settings::UpstreamProxyDomain()
{
    if (DoesSettingExist(UPSTREAM_PROXY_DOMAIN_NAME))
    {
        return GetSettingString(UPSTREAM_PROXY_DOMAIN_NAME, UPSTREAM_PROXY_DOMAIN_DEFAULT);
    }

    // Check the defunct key, as we might have to migrate the old value
    string domain;
    string upstreamProxyAuthenticatedHostname = GetSettingString(UPSTREAM_PROXY_HOSTNAME_NAME_DEFUNCT, UPSTREAM_PROXY_HOSTNAME_DEFAULT_DEFUNCT);

    int splitIndex = upstreamProxyAuthenticatedHostname.find_first_of("\\");
    if (splitIndex != string::npos) {
        domain = upstreamProxyAuthenticatedHostname.substr(0, splitIndex);
    }

    // Attempt to write the extracted value to the new key, so we don't have to do this every time.
    RegistryFailureReason failureReason; // ignored, along with return value -- if this write fails we'll do it again next time
    (void)WriteRegistryStringValue(UPSTREAM_PROXY_DOMAIN_NAME, domain, failureReason);

    return domain;
}

string Settings::UpstreamProxyFullHostname()
{
    // Assumes HTTP proxy

    ostringstream ret;

    if (!UpstreamProxyHostname().empty())
    {
        ret << "http://";
    }

    if (!UpstreamProxyDomain().empty())
    {
        ret << psicash::URL::Encode(UpstreamProxyDomain(), false) << "\\";
    }

    if (!UpstreamProxyUsername().empty())
    {
        ret << psicash::URL::Encode(UpstreamProxyUsername(), false);
    }

    if (!UpstreamProxyPassword().empty())
    {
        ret << ":" << psicash::URL::Encode(UpstreamProxyPassword(), false);
    }

    if (!UpstreamProxyHostname().empty())
    {
        if (!UpstreamProxyUsername().empty())
        {
            ret << "@";
        }
        ret << UpstreamProxyHostname();
    }

    if (UpstreamProxyPort() != 0)
    {
        ret << ":" << UpstreamProxyPort();
    }

    return ret.str();
}

bool Settings::SkipUpstreamProxy()
{
    return !!GetSettingDword(SKIP_UPSTREAM_PROXY_NAME, SKIP_UPSTREAM_PROXY_DEFAULT);
}

string Settings::EgressRegion()
{
    return GetSettingString(EGRESS_REGION_NAME, EGRESS_REGION_DEFAULT);
}

bool Settings::SystrayMinimize()
{
    return !!GetSettingDword(SYSTRAY_MINIMIZE_NAME, SYSTRAY_MINIMIZE_DEFAULT);
}

bool Settings::DisableDisallowedTrafficAlert()
{
    return !!GetSettingDword(DISABLE_DISALLOWED_TRAFFIC_ALERT_NAME, DISABLE_DISALLOWED_TRAFFIC_ALERT_DEFAULT);
}

/*
Settings that are not exposed in the UI.
*/

bool Settings::SkipProxySettings()
{
    return !!GetSettingDword(SKIP_PROXY_SETTINGS_NAME, SKIP_PROXY_SETTINGS_DEFAULT);
}

bool Settings::SkipAutoConnect()
{
    return !!GetSettingDword(SKIP_AUTO_CONNECT_NAME, SKIP_AUTO_CONNECT_DEFAULT);
}

/*
For internal use only
TODO: Probably shouldn't be in the "usersettings" file
*/

void Settings::SetCookies(const string& value)
{
    RegistryFailureReason reason = REGISTRY_FAILURE_NO_REASON;
    (void)WriteRegistryStringValue(COOKIES_NAME, value, reason);
    // ignoring failures
}

string Settings::GetCookies()
{
    return GetSettingString(COOKIES_NAME, COOKIES_DEFAULT);
}

void Settings::SetWindowPlacement(const string& value)
{
    RegistryFailureReason reason = REGISTRY_FAILURE_NO_REASON;
    (void)WriteRegistryStringValue(WINDOW_PLACEMENT_NAME, value, reason);
    // ignoring failures
}

string Settings::GetWindowPlacement()
{
    return GetSettingString(WINDOW_PLACEMENT_NAME, WINDOW_PLACEMENT_DEFAULT);
}
