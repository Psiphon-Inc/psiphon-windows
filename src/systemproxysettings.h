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
    static const int INTERNET_OPTIONS_NUMBER = 3;

    typedef vector<connection_proxy>::iterator connection_proxy_iter;
    typedef vector<tstring>::const_iterator tstring_iter;

    void PreviousCrashCheckHack(connection_proxy& proxySettings);
    bool Save(const vector<tstring>& connections);
    bool SetConnectionsProxies(const vector<tstring>& connections, const tstring& proxyAddress);
    bool SetConnectionProxy(const connection_proxy& setting);
    bool GetConnectionProxy(connection_proxy& setting);
    vector<tstring> GetRasConnectionNames();
    tstring MakeProxySettingString();

    bool m_settingsApplied;
    vector<connection_proxy> m_originalSettings;

    int m_httpProxyPort;
    int m_httpsProxyPort;
    int m_socksProxyPort;
};
