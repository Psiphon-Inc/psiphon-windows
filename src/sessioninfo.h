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
#include "vpnlist.h"
#include "tstring.h"

class ConnectionManager;

struct RegexReplace
{
    regex regex;
    string replace;
};

class SessionInfo
{
public:
    void Set(const ServerEntry& serverEntry);

    string GetServerAddress(void) const {return m_serverEntry.serverAddress;}
    int GetWebPort(void) const {return m_serverEntry.webServerPort;}
    string GetWebServerSecret(void) const {return m_serverEntry.webServerSecret;}
    string GetWebServerCertificate(void) const { return m_serverEntry.webServerCertificate;}
    string GetUpgradeVersion(void) const {return m_upgradeVersion;}
    string GetPSK(void) const {return m_psk;}
    string GetSSHPort(void) const {return m_sshPort;}
    string GetSSHUsername(void) const {return m_sshUsername;}
    string GetSSHPassword(void) const {return m_sshPassword;}
    string GetSSHHostKey(void) const {return m_sshHostKey;}
    string GetSSHSessionID(void) const {return m_sshSessionID;}
    string GetSSHObfuscatedPort(void) const {return m_sshObfuscatedPort;}
    string GetSSHObfuscatedKey(void) const {return m_sshObfuscatedKey;}
    vector<tstring> GetHomepages(void) const {return m_homepages;}
    vector<string> GetDiscoveredServerEntries(void) const {return m_servers;}
    vector<RegexReplace> GetPageViewRegexes() const {return m_pageViewRegexes;}
    vector<RegexReplace> GetHttpsRequestRegexes() const {return m_httpsRequestRegexes;}
    string GetSpeedTestServerAddress(void) const {return m_speedTestServerAddress;}
    string GetSpeedTestServerPort(void) const {return m_speedTestServerPort;}
    string GetSpeedTestRequestPath(void) const {return m_speedTestRequestPath;}

    bool ParseHandshakeResponse(const string& response);
    bool ProcessConfig(const string& config_json);

private:
    ServerEntry m_serverEntry;

    string m_upgradeVersion;
    string m_psk;
    string m_sshPort;
    string m_sshUsername;
    string m_sshPassword;
    string m_sshHostKey;
    string m_sshSessionID;
    string m_sshObfuscatedPort;
    string m_sshObfuscatedKey;
    vector<tstring> m_homepages;
    vector<string> m_servers;
    vector<RegexReplace> m_pageViewRegexes;
    vector<RegexReplace> m_httpsRequestRegexes;
    string m_speedTestServerAddress;
    string m_speedTestServerPort;
    string m_speedTestRequestPath;
};
