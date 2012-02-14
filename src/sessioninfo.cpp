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
#include "psiclient.h"
#include <sstream>


void SessionInfo::Set(const ServerEntry& serverEntry)
{
    m_serverEntry = serverEntry;
}

bool SessionInfo::ParseHandshakeResponse(const string& response)
{
    // Expected response:
    //
    // Upgrade: <url> \n        (zero or one)
    // PSK: <hexstring>\n       (zero or one)
    // HomePage: <url>\n        (zero or more)
    // Server: <hexstring>\n    (zero or more)
    // SSHPort: <string>\n         (zero or one)
    // SSHUsername: <string>\n  (zero or one)
    // SSHPassword: <string>\n  (zero or one)
    // SSHHostKey: <string>\n   (zero or one)
    // SSHSessionID: <string>\n   (zero or one)
    // Config: <json>\n   (one)

    static const char* UPGRADE_PREFIX = "Upgrade: ";
    static const char* PSK_PREFIX = "PSK: ";
    static const char* SSH_PORT_PREFIX = "SSHPort: ";
    static const char* SSH_USERNAME_PREFIX = "SSHUsername: ";
    static const char* SSH_PASSWORD_PREFIX = "SSHPassword: ";
    static const char* SSH_HOST_KEY_PREFIX = "SSHHostKey: ";
    static const char* SSH_SESSION_ID_PREFIX = "SSHSessionID: ";
    static const char* SSH_OBFUSCATED_PORT_PREFIX = "SSHObfuscatedPort: ";
    static const char* SSH_OBFUSCATED_KEY_PREFIX = "SSHObfuscatedKey: ";
    static const char* HOMEPAGE_PREFIX = "Homepage: ";
    static const char* SERVER_PREFIX = "Server: ";
    static const char* CONFIG_PREFIX = "Config: ";

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

    stringstream stream(response);
    string item;

    while (getline(stream, item, '\n'))
    {
        if (0 == item.find(UPGRADE_PREFIX))
        {
            item.erase(0, strlen(UPGRADE_PREFIX));
            m_upgradeVersion = item;
        }
        else if (0 == item.find(PSK_PREFIX))
        {
            item.erase(0, strlen(PSK_PREFIX));
            m_psk = item;
        }
        else if (0 == item.find(SSH_PORT_PREFIX))
        {
            item.erase(0, strlen(SSH_PORT_PREFIX));
            m_sshPort = atoi(item.c_str());
        }
        else if (0 == item.find(SSH_USERNAME_PREFIX))
        {
            item.erase(0, strlen(SSH_USERNAME_PREFIX));
            m_sshUsername = item;
        }
        else if (0 == item.find(SSH_PASSWORD_PREFIX))
        {
            item.erase(0, strlen(SSH_PASSWORD_PREFIX));
            m_sshPassword = item;
        }
        else if (0 == item.find(SSH_HOST_KEY_PREFIX))
        {
            item.erase(0, strlen(SSH_HOST_KEY_PREFIX));
            m_sshHostKey = item;
        }
        else if (0 == item.find(SSH_SESSION_ID_PREFIX))
        {
            item.erase(0, strlen(SSH_SESSION_ID_PREFIX));
            m_sshSessionID = item;
        }
        else if (0 == item.find(SSH_OBFUSCATED_PORT_PREFIX))
        {
            item.erase(0, strlen(SSH_OBFUSCATED_PORT_PREFIX));
            m_sshObfuscatedPort = atoi(item.c_str());
        }
        else if (0 == item.find(SSH_OBFUSCATED_KEY_PREFIX))
        {
            item.erase(0, strlen(SSH_OBFUSCATED_KEY_PREFIX));
            m_sshObfuscatedKey = item;
        }
        else if (0 == item.find(HOMEPAGE_PREFIX))
        {
            item.erase(0, strlen(HOMEPAGE_PREFIX));
            m_homepages.push_back(NarrowToTString(item));
        }
        else if  (0 == item.find(SERVER_PREFIX))
        {
            item.erase(0, strlen(SERVER_PREFIX));
            m_servers.push_back(item);
        }
        else if (0 == item.find(CONFIG_PREFIX))
        {
            item.erase(0, strlen(CONFIG_PREFIX));
            if (!ProcessConfig(item))
            {
                return false;
            }
        }
    }
    // TODO: more explicit validation?  Eg, got exactly one non-blank PSK

    return true;
}

bool SessionInfo::ProcessConfig(const string& config_json)
{
    m_pageViewRegexes.clear();
    m_httpsRequestRegexes.clear();
    m_speedTestServerAddress.clear();
    m_speedTestServerPort.clear();
    m_speedTestRequestPath.clear();

    Json::Value config;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(config_json, config);
    if (!parsingSuccessful)
    {
        string fail = reader.getFormattedErrorMessages();
        my_print(false, _T("%s:%d: Page view regex parse failed: %S"), __TFUNCTION__, __LINE__, reader.getFormattedErrorMessages().c_str());
        return false;
    }

    try
    {
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

        // Speed Test URL

        Json::Value speedTestURL = config["speed_test_url"];
        if (Json::Value::null != speedTestURL)
        {
            m_speedTestServerAddress = speedTestURL.get("server_address", "").asString();
            m_speedTestServerPort = speedTestURL.get("server_port", "").asString();
            m_speedTestRequestPath = speedTestURL.get("request_path", "").asString();
        }
    }
    catch (exception& e)
    {
        my_print(false, _T("%s:%d: Config parse exception: %S"), __TFUNCTION__, __LINE__, e.what());
        return false;
    }

    return true;
}

string SessionInfo::GetServerAddress() const 
{
    return m_serverEntry.serverAddress;
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

