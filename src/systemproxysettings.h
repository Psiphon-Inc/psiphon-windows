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

#pragma once

#include "tstring.h"

#include <vector>

using namespace std;

class SystemProxySettings
{
public:
    SystemProxySettings(void);
    virtual ~SystemProxySettings(void);
    bool Configure(void);
    bool Revert(void);

private:
    static const int INTERNET_OPTIONS_NUMBER = 2;

    struct connection_proxy
    {
        tstring name;
        DWORD flags;
        tstring proxy;
    };

    typedef vector<connection_proxy>::iterator connection_proxy_iter;
    typedef vector<tstring>::const_iterator tstring_iter;

    void PreviousCrashCheckHack(connection_proxy& proxySettings);
    bool Save(const vector<tstring>& connections);
    bool SetConnectionsProxies(const vector<tstring>& connections, const tstring& proxyAddress);
    bool SetConnectionProxy(const connection_proxy& setting);
    bool GetConnectionProxy(connection_proxy& setting);
    vector<tstring> GetRasConnectionNames(void);

    vector<connection_proxy> m_originalSettings;
};
