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

#pragma once

#include "tstring.h"
#include <vector>

using namespace std;

struct ConnectionProxy;


class SystemProxySettings
{
public:
    SystemProxySettings();
    virtual ~SystemProxySettings();

    int GetHttpProxyPort() const;
    int GetHttpsProxyPort() const;
    int GetSocksProxyPort() const;

    // The Set*ProxyPort functions do *not* apply the setting -- they only set
    // a member variable that will be used when Apply is called to actually
    // make the settings take effect.
    void SetHttpProxyPort(int port);
    void SetHttpsProxyPort(int port);
    void SetSocksProxyPort(int port);

    int GetHttpProxyPort() { return m_httpProxyPort; }

    bool Apply(bool allowedToSkipProxySettings);
    bool Revert();
    bool IsApplied() const;

private:
    tstring MakeProxySettingString() const;

    bool m_settingsApplied;

    int m_httpProxyPort;
    int m_httpsProxyPort;
    int m_socksProxyPort;
};


struct ConnectionProxy
{
    tstring name;
    DWORD flags; // combo of: PROXY_TYPE_DIRECT, PROXY_TYPE_PROXY, PROXY_TYPE_AUTO_PROXY_URL, PROXY_TYPE_AUTO_DETECT
    tstring flagsString;
    tstring proxy;
    tstring bypass;

    ConnectionProxy() : flags(0) {}

    bool operator==(const ConnectionProxy& rhs)
    {
        return
            this->name == rhs.name &&
            this->flags == rhs.flags &&
            this->proxy == rhs.proxy &&
            this->bypass == rhs.bypass;
    }

    bool operator!=(const ConnectionProxy& rhs)
    {
        return !(*this == rhs);
    }

    void clear()
    {
        this->name.clear();
        flags = 0;
        this->flagsString.clear();
        this->proxy.clear();
        this->bypass.clear();
    }
};


class ProxyConfig
{
public:
    ProxyConfig() { Clear(); }
    void Clear()
    {
        https.clear();
        httpsPort = 0;
        socksProxy.clear();
        socksProxyPort = 0;
    }

    static ProxyConfig DecomposeProxyInfo(const ConnectionProxy& proxyInfo);

    /// Returns true if there is an HTTP proxy to use.
    bool HTTPEnabled() const;
    /// Returns the hostname to be used for an HTTP proxy connection, or an 
    /// empty string if no proxy should be used.
    tstring HTTPHostname() const;
    /// Returns the server port to be used for an HTTP proxy connection, or zero
    /// if no port should be used.
    DWORD HTTPPort() const;
    /// Returns the `hostname:port` to be used for an HTTP proxy connection, or 
    /// an empty string if no proxy should be used.
    tstring HTTPHostPort() const;
    /// Returns the `scheme://hostname:port` to be used for an HTTP proxy 
    /// connection, or an empty string if no proxy should be used.
    tstring HTTPHostPortScheme() const;

private:
    /// HTTP proxy to be used for HTTPS traffic. This is what we will use for
    /// our "HTTP" proxy, as we can be sure it supports CONNECT.
    tstring https;
    DWORD httpsPort;
    
    /// SOCKS proxy to be used for all traffic
    tstring socksProxy;
    DWORD socksProxyPort;
};


/// Get the proxy config for the current tunnel
ProxyConfig GetTunneledDefaultProxyConfig();
/// Get the proxy info for the original default connection.
ProxyConfig GetNativeDefaultProxyConfig();


void DoStartupSystemProxyWork();
void GetSanitizedOriginalProxyInfo(vector<ConnectionProxy>& o_originalProxyInfo);
