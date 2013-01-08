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

struct connection_proxy;


class SystemProxySettings
{
public:
    SystemProxySettings();
    virtual ~SystemProxySettings();

    //
    // The Set*ProxyPort functions do *not* apply the setting -- they only set
    // a member variable that will be used when Apply is called to actually
    // make the settings take effect.
    void SetHttpProxyPort(int port);
    void SetHttpsProxyPort(int port);
    void SetSocksProxyPort(int port);
    bool Apply();

    bool Revert();

private:
    typedef vector<connection_proxy>::iterator connection_proxy_iter;

    void PreviousCrashCheckHack(connection_proxy& proxySettings);
    bool Save(const vector<connection_proxy>& proxyInfo);
    bool SetConnectionsProxies(const vector<tstring>& connections, const tstring& proxyAddress);
    tstring MakeProxySettingString();

    bool m_settingsApplied;
    vector<connection_proxy> m_originalSettings;

    int m_httpProxyPort;
    int m_httpsProxyPort;
    int m_socksProxyPort;
};


struct ConnectionProxyInfo
{
    tstring connectionName;
    tstring flags;
    tstring proxy;
    tstring bypass;
};

void GetOriginalProxyInfo(vector<ConnectionProxyInfo>& originalProxyInfo);
