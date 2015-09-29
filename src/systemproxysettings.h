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


struct DecomposedProxyConfig
{
    tstring httpProxy;
    DWORD httpProxyPort;
    tstring httpsProxy;
    DWORD httpsProxyPort;
    tstring socksProxy;
    DWORD socksProxyPort;

    void clear()
    {
        httpProxy.clear();
        httpProxyPort = 0;
        httpsProxy.clear();
        httpsProxyPort = 0;
        socksProxy.clear();
        socksProxyPort = 0;
    }
};


void DoStartupSystemProxyWork();
tstring GetNativeDefaultHttpsProxyHost();
tstring GetTunneledDefaultHttpsProxyHost();
void GetNativeDefaultProxyInfo(DecomposedProxyConfig& o_proxyInfo);
void GetSanitizedOriginalProxyInfo(vector<ConnectionProxy>& o_originalProxyInfo);
