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

#include "stdafx.h"
#include "sessioninfo.h"
#include "logging.h"
#include "psiclient.h"
#include "config.h"
#include "utilities.h"
#include <sstream>


// This value determines whether or not we will perform preemptive reconnect
// behaviour if the handshake fails. If it's zero (or MAXDWORD, really), then 
// we won't, if it's something like 60000, then we will.
#define PREEMPTIVE_RECONNECT_LIFETIME_MILLISECONDS_DEFAULT MAXDWORD


SessionInfo::SessionInfo()
{
    Clear();
}

void SessionInfo::Clear()
{
    m_clientSessionID.clear();
    m_upgradeVersion.clear();
    m_psk.clear();
    m_sshPort = 0;
    m_sshUsername.clear();
    m_sshPassword.clear();
    m_sshHostKey.clear();
    m_sshSessionID.clear();
    m_sshObfuscatedPort = 0;
    m_sshObfuscatedKey.clear();
    m_meekObfuscatedKey.clear();
    m_meekServerPort = 0;
    m_meekCookieEncryptionPublicKey.clear();
    m_meekFrontingDomain.clear();
    m_meekFrontingHost.clear();
    m_homepages.clear();
    m_servers.clear();
    m_pageViewRegexes.clear();
    m_httpsRequestRegexes.clear();
    m_preemptiveReconnectLifetimeMilliseconds = PREEMPTIVE_RECONNECT_LIFETIME_MILLISECONDS_DEFAULT;
    m_localHttpProxyPort = 0;
    m_localHttpsProxyPort = 0;
    m_localSocksProxyPort = 0;
}

void SessionInfo::Set(const ServerEntry& serverEntry, bool generateClientSessionID/*=true*/)
{
    Clear();

    m_serverEntry = serverEntry;

    if (generateClientSessionID)
    {
        GenerateClientSessionID();
    }
}

void SessionInfo::GenerateClientSessionID()
{
    unsigned char bytes[CLIENT_SESSION_ID_BYTES];
    assert(CLIENT_SESSION_ID_BYTES % sizeof(unsigned int) == 0);
    for (int i=0; i < CLIENT_SESSION_ID_BYTES/sizeof(unsigned int); i++)
    {
        rand_s(((unsigned int*)bytes) + i);
    }
    m_clientSessionID = Hexlify(bytes, CLIENT_SESSION_ID_BYTES);
}

bool SessionInfo::ParseHandshakeResponse(const string& response)
{
    // The handshake response will contain a bunch of legacy fields for
    // backward-compatibility, and then a Config field with a JSON object
    // holding the information we want.

    static const char* CONFIG_PREFIX = "Config: ";

    stringstream stream(response);
    string item;

    while (getline(stream, item, '\n'))
    {
        if (0 == item.find(CONFIG_PREFIX))
        {
            item.erase(0, strlen(CONFIG_PREFIX));
            if (!ProcessConfig(item))
            {
                return false;
            }
            break;
        }
    }

    return true;
}

bool SessionInfo::ProcessConfig(const string& config_json)
{
    m_upgradeVersion.clear();
    m_psk.clear();
    m_sshPort = 0;
    m_sshUsername.clear();
    m_sshPassword.clear();
    m_sshHostKey.clear();
    m_sshSessionID.clear();
    m_sshObfuscatedPort = 0;
    m_sshObfuscatedKey.clear();
    m_homepages.clear();
    m_servers.clear();
    m_pageViewRegexes.clear();
    m_httpsRequestRegexes.clear();
    m_preemptiveReconnectLifetimeMilliseconds = PREEMPTIVE_RECONNECT_LIFETIME_MILLISECONDS_DEFAULT;
    m_localHttpProxyPort = 0;
    m_localHttpsProxyPort = 0;
    m_localSocksProxyPort = 0;

    Json::Value config;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(config_json, config);
    if (!parsingSuccessful)
    {
        string fail = reader.getFormattedErrorMessages();
        my_print(NOT_SENSITIVE, false, _T("%s:%d: Page view regex parse failed: %S"), __TFUNCTION__, __LINE__, fail.c_str());
        return false;
    }

    try
    {
        // Homepages
        Json::Value homepages = config["homepages"];
        for (Json::Value::ArrayIndex i = 0; i < homepages.size(); i++)
        {
            m_homepages.push_back(UTF8ToWString(homepages[i].asString()));
        }

        // Upgrade
        m_upgradeVersion = config.get("upgrade_client_version", "").asString();

        // Servers
        Json::Value servers = config["encoded_server_list"];
        for (Json::Value::ArrayIndex i = 0; i < servers.size(); i++)
        {
            m_servers.push_back(servers[i].asString());
        }

        // SSH and OSSH values
        m_sshPort = config.get("ssh_port", 0).asInt();
        m_sshUsername = config.get("ssh_username", "").asString();
        m_sshPassword = config.get("ssh_password", "").asString();
        m_sshHostKey = config.get("ssh_host_key", "").asString();
        m_sshSessionID = config.get("ssh_session_id", "").asString();
        m_sshObfuscatedPort = config.get("ssh_obfuscated_port", 0).asInt();
        m_sshObfuscatedKey = config.get("ssh_obfuscated_key", "").asString();

        // VPN PSK
        m_psk = config.get("l2tp_ipsec_psk", "").asString();

        // Page view regexes        
        Json::Value regexes = config["page_view_regexes"];
        for (Json::Value::ArrayIndex i = 0; i < regexes.size(); i++)
        {
            RegexReplace rx_re;
            rx_re.regex = regex(
                            regexes[i].get("regex", "").asString(), 
                            regex::ECMAScript | regex::icase | regex::optimize);
            rx_re.replace = regexes[i].get("replace", "").asString();

            m_pageViewRegexes.push_back(rx_re);
        }

        // HTTPS request regexes        
        regexes = config["https_request_regexes"];
        for (Json::Value::ArrayIndex i = 0; i < regexes.size(); i++)
        {
            RegexReplace rx_re;
            rx_re.regex = regex(
                            regexes[i].get("regex", "").asString(), 
                            regex::ECMAScript | regex::icase | regex::optimize);
            rx_re.replace = regexes[i].get("replace", "").asString();

            m_httpsRequestRegexes.push_back(rx_re);
        }

        // Preemptive Reconnect Lifetime Milliseconds
        m_preemptiveReconnectLifetimeMilliseconds = (DWORD)config.get("preemptive_reconnect_lifetime_milliseconds", 0).asUInt();
        // A zero value indicates that it should be disabled.
    }
    catch (exception& e)
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d: Config parse exception: %S"), __TFUNCTION__, __LINE__, e.what());
        return false;
    }

    return true;
}

void SessionInfo::SetHomepage(const char* homepage)
{
    tstring newHomepage = UTF8ToWString(homepage);
    if (m_homepages.end() == std::find(m_homepages.begin(), m_homepages.end(), newHomepage))
    {
        m_homepages.push_back(newHomepage);
    }
}

void SessionInfo::SetUpgradeVersion(const char* upgradeVersion)
{
    m_upgradeVersion = upgradeVersion;
}

void SessionInfo::SetLocalProxyPorts(int http, int https, int socks)
{
    m_localHttpProxyPort = http;
    m_localHttpsProxyPort = https;
    m_localSocksProxyPort = socks;
}

string SessionInfo::GetServerAddress() const 
{
    return m_serverEntry.serverAddress;
}

string SessionInfo::GetRegion() const
{
    return m_serverEntry.region;
}

int SessionInfo::GetWebPort() const
{
    return m_serverEntry.webServerPort;
}

string SessionInfo::GetWebServerSecret() const 
{
    return m_serverEntry.webServerSecret;
}

string SessionInfo::GetWebServerCertificate() const 
{ 
    return m_serverEntry.webServerCertificate;
}

// Returns the first value that is greater than zero
static int Coalesce(int x, int y)
{
    return x > 0 ? x : y;
}

// Returns the first value that is non-empty.
static string Coalesce(const string& x, const string& y)
{
    return !x.empty() ? x : y;
}

int SessionInfo::GetSSHPort() const 
{
    return Coalesce(m_sshPort, m_serverEntry.sshPort);
}

string SessionInfo::GetSSHUsername() const 
{
    return Coalesce(m_sshUsername, m_serverEntry.sshUsername);
}

string SessionInfo::GetSSHPassword() const 
{
    return Coalesce(m_sshPassword, m_serverEntry.sshPassword);
}

string SessionInfo::GetSSHHostKey() const 
{
    return Coalesce(m_sshHostKey, m_serverEntry.sshHostKey);
}

int SessionInfo::GetSSHObfuscatedPort() const 
{
    return Coalesce(m_sshObfuscatedPort, m_serverEntry.sshObfuscatedPort);
}

string SessionInfo::GetSSHObfuscatedKey() const 
{
    return Coalesce(m_sshObfuscatedKey, m_serverEntry.sshObfuscatedKey);
}

string SessionInfo::GetMeekObfuscatedKey() const
{
    return Coalesce(m_meekObfuscatedKey, m_serverEntry.meekObfuscatedKey);
}

int SessionInfo::GetMeekServerPort() const
{
    return Coalesce(m_meekServerPort, m_serverEntry.meekServerPort);
}

string SessionInfo::GetMeekFrontingDomain() const
{
    return Coalesce(m_meekFrontingDomain, m_serverEntry.meekFrontingDomain);
}

string SessionInfo::GetMeekFrontingHost() const
{
    return Coalesce(m_meekFrontingHost, m_serverEntry.meekFrontingHost);
}

string SessionInfo::GetMeekCookieEncryptionPublicKey() const
{
    return Coalesce(m_meekCookieEncryptionPublicKey, m_serverEntry.meekCookieEncryptionPublicKey);
}

vector<string> SessionInfo::GetDiscoveredServerEntries() const 
{
    return m_servers;
}

bool SessionInfo::HasServerEntry() const
{
    return m_serverEntry.serverAddress.length() > 0;
}

ServerEntry SessionInfo::GetServerEntry() const 
{
    if (!HasServerEntry()) {
        assert(false);
        throw std::exception("SessionInfo::GetServerEntry(): !HasServerEntry()");
    }

    // It is sometimes the case that we know more about our current server than
    // is contained in m_serverEntry or in the ServerEntry in the registry. So
    // we'll construct a new ServerEntry with the best info we have.

    ServerEntry newServerEntry(
        GetServerAddress(), GetRegion(), GetWebPort(), 
        GetWebServerSecret(), GetWebServerCertificate(), 
        GetSSHPort(), GetSSHUsername(), GetSSHPassword(), 
        GetSSHHostKey(), GetSSHObfuscatedPort(), 
        GetSSHObfuscatedKey(),
        GetMeekObfuscatedKey(), GetMeekServerPort(),
        GetMeekCookieEncryptionPublicKey(),
        GetMeekFrontingDomain(), GetMeekFrontingHost(),
        m_serverEntry.meekFrontingAddressesRegex,
        m_serverEntry.meekFrontingAddresses,
        m_serverEntry.capabilities);
    return newServerEntry;
}
