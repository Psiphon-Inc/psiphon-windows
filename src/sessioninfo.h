/*
 * Copyright (c) 2011, Psiphon Inc.
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

#include <vector>
#include "serverlist.h"
#include "tstring.h"

struct RegexReplace
{
    regex regex;
    string replace;
};

class SessionInfo
{
public:
    SessionInfo();

    void Set(const ServerEntry& serverEntry, bool generateClientSessionID=true);

    // Generate a new client session ID to be included with subsequent web requests
    void GenerateClientSessionID();

    string GetClientSessionID() const {return m_clientSessionID;}

    string GetServerAddress() const;
    string GetRegion() const;
    int GetWebPort() const;
    string GetWebServerSecret() const;
    string GetWebServerCertificate() const;
    int GetSSHPort() const;
    string GetSSHUsername() const;
    string GetSSHPassword() const;
    string GetSSHHostKey() const;
    int GetSSHObfuscatedPort() const;
    string GetSSHObfuscatedKey() const;

    string GetMeekObfuscatedKey() const;
    int GetMeekServerPort() const;
    string GetMeekFrontingDomain() const;
    string GetMeekFrontingHost() const;
    string GetMeekCookieEncryptionPublicKey() const;

    string GetSSHSessionID() const {return m_sshSessionID;}
    string GetUpgradeVersion() const {return m_upgradeVersion;}
    string GetPSK() const {return m_psk;}
    vector<tstring> GetHomepages() const {return m_homepages;}
    vector<string> GetDiscoveredServerEntries() const;
    vector<RegexReplace> GetPageViewRegexes() const {return m_pageViewRegexes;}
    vector<RegexReplace> GetHttpsRequestRegexes() const {return m_httpsRequestRegexes;}
    
    // A value of zero means disabled.
    DWORD GetPreemptiveReconnectLifetimeMilliseconds() const {return m_preemptiveReconnectLifetimeMilliseconds;}

    int GetLocalHttpProxyPort() const { return m_localHttpProxyPort; }
    int GetLocalHttpsProxyPort() const { return m_localHttpsProxyPort; }
    int GetLocalSocksProxyPort() const { return m_localSocksProxyPort; }
    
    // Will be false if Set() never called with a valid ServerEntry.
    bool HasServerEntry() const;
    // Will throw exception if this instance does not have a ServerEntry.
    ServerEntry GetServerEntry() const;

    bool ParseHandshakeResponse(const string& response);
    bool ProcessConfig(const string& config_json);

    void SetHomepage(const char* homepage);
    void SetUpgradeVersion(const char* upgradeVersion);

    void SetLocalProxyPorts(int http, int https, int socks);

protected:
    void Clear();

private:
    ServerEntry m_serverEntry;

    string m_clientSessionID;
    string m_upgradeVersion;
    string m_psk;
    int m_sshPort;
    string m_sshUsername;
    string m_sshPassword;
    string m_sshHostKey;
    string m_sshSessionID;
    int m_sshObfuscatedPort;
    string m_sshObfuscatedKey;
    string m_meekObfuscatedKey;
    int m_meekServerPort;
    string m_meekCookieEncryptionPublicKey;
    string m_meekFrontingDomain;
    string m_meekFrontingHost;
    vector<tstring> m_homepages;
    vector<string> m_servers;
    vector<RegexReplace> m_pageViewRegexes;
    vector<RegexReplace> m_httpsRequestRegexes;
    DWORD m_preemptiveReconnectLifetimeMilliseconds;
    int m_localHttpProxyPort;
    int m_localHttpsProxyPort;
    int m_localSocksProxyPort;
};
