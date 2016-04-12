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
#include "logging.h"
#include "psiclient.h"
#include "serverlist.h"
#include "embeddedvalues.h"
#include "config.h"
#include "utilities.h"
#include <algorithm>
#include <sstream>


ServerList::ServerList(LPCSTR listName)
{
    assert(listName && strlen(listName));
    m_name = listName;

    // Used a named mutex, because we'll need to use the mutex across instances.
    tstring mutexName = _T("Local\\ServerListMutex-") + UTF8ToWString(listName);
    m_mutex = CreateMutex(NULL, FALSE, mutexName.c_str());
}

ServerList::~ServerList()
{
    CloseHandle(m_mutex);
}

// This function may throw
size_t ServerList::AddEntriesToList(
                    const vector<string>& newServerEntryList,
                    const ServerEntry* serverEntry)
{
    AutoMUTEX lock(m_mutex);

    size_t entriesAdded = 0;

    if (newServerEntryList.size() < 1 && !serverEntry)
    {
        return entriesAdded;
    }

    // We're going to loop through the server entries twice -- once to decode
    // them and once to process them (after adding in serverEntry). This is
    // not optimally efficient, but the array will never be very large and the
    // code will be cleaner.

    vector<ServerEntry> decodedServerEntries;
    vector<string>::const_iterator entryStringIter;
    for (entryStringIter = newServerEntryList.begin();
         entryStringIter != newServerEntryList.end(); 
         ++entryStringIter)
    {
        decodedServerEntries.push_back(ParseServerEntry(*entryStringIter));
    }

    if (serverEntry) decodedServerEntries.push_back(*serverEntry);

    // This list may contain more than one discovered server
    // Randomize this list for load-balancing
    random_shuffle(decodedServerEntries.begin(), decodedServerEntries.end());

    ServerEntries oldServerEntryList = GetList();
    
    vector<ServerEntry>::const_iterator decodedEntryIter;
    for (decodedEntryIter = decodedServerEntries.begin();
         decodedEntryIter != decodedServerEntries.end(); ++decodedEntryIter)
    {
        // Check if we already know about this server
        bool alreadyKnown = false;
        for (ServerEntries::iterator oldServerEntry = oldServerEntryList.begin();
             oldServerEntry != oldServerEntryList.end(); ++oldServerEntry)
        {
            if ((*decodedEntryIter).serverAddress == oldServerEntry->serverAddress)
            {
                alreadyKnown = true;
                // NOTE: We always update the values for known servers, because we trust the
                //       discovery mechanisms
                oldServerEntry->Copy(*decodedEntryIter);
                break;
            }
        }

        if (!alreadyKnown)
        {
            // Insert the new entry as the second entry, so that the first entry can continue
            // to be used if it is reachable (unless there are no pre-existing entries).
            oldServerEntryList.insert(
                oldServerEntryList.size() == 0 ? 
                    oldServerEntryList.begin() :
                    oldServerEntryList.begin() + 1, 
                *decodedEntryIter);

            entriesAdded++;
        }
    }

    WriteListToSystem(oldServerEntryList);

    return entriesAdded;
}

void ServerList::MoveEntriesToFront(const ServerEntries& entries, bool veryFront/*=false*/)
{
    AutoMUTEX lock(m_mutex);

    ServerEntries persistentServerEntryList = GetList();

    // Insert entries in input order

    for (ServerEntries::const_reverse_iterator entry = entries.rbegin(); entry != entries.rend(); ++entry)
    {
        // Remove existing entry for server, if present. In the case where
        // the existing entry has different data, we must assume that a
        // discovery has happened that overwrote the data that's being
        // passed in. In that edge case, we just keep the existing entry
        // in its current position.

        bool existingEntryChanged = false;

        // If we replace the head item, we want to make sure we insert at the head.
        bool forceHead = false;

        for (ServerEntries::iterator persistentEntry = persistentServerEntryList.begin();
             persistentEntry != persistentServerEntryList.end();
             ++persistentEntry)
        {
            if (entry->serverAddress == persistentEntry->serverAddress)
            {
                if (entry->ToString() != persistentEntry->ToString())
                {
                    existingEntryChanged = true;
                }
                else
                {
                    forceHead = (persistentEntry == persistentServerEntryList.begin());
                    persistentServerEntryList.erase(persistentEntry);
                }
                break;
            }
        }

        // Re-insert entry for server in new position

        if (!existingEntryChanged)
        {
            ServerEntries::const_iterator insertionPoint;
            if (veryFront || forceHead || persistentServerEntryList.size() == 0)
            {
                insertionPoint = persistentServerEntryList.begin();
            }
            else
            {
                insertionPoint = persistentServerEntryList.begin() + 1;
            }

            persistentServerEntryList.insert(insertionPoint, *entry);
        }
    }

    WriteListToSystem(persistentServerEntryList);
}

void ServerList::MoveEntryToFront(const ServerEntry& serverEntry, bool veryFront/*=false*/)
{
    ServerEntries serverEntries;
    serverEntries.push_back(serverEntry);
    MoveEntriesToFront(serverEntries, veryFront);
}

void ServerList::MarkServersFailed(const ServerEntries& failedServerEntries)
{
    AutoMUTEX lock(m_mutex);

    ServerEntries serverEntryList = GetList();
    if (serverEntryList.size() == 0 || failedServerEntries.size() == 0)
    {
        return;
    }

    my_print(NOT_SENSITIVE, true, _T("%s: Marking %d servers failed"), __TFUNCTION__, failedServerEntries.size());

    bool changeMade = false;

    for (ServerEntries::const_iterator failed = failedServerEntries.begin();
            failed != failedServerEntries.end();
            ++failed)
    {
        for (ServerEntries::iterator entry = serverEntryList.begin();
             entry != serverEntryList.end();
             ++entry)
        {
            if (failed->serverAddress == entry->serverAddress)
            {
                ServerEntry failedServer;
                failedServer.Copy(*entry);

                // Move the failed server to the end of the list
                serverEntryList.erase(entry);
                serverEntryList.push_back(failedServer);

                changeMade = true;
                break;
            }
        }
    }

    if (changeMade)
    {
        WriteListToSystem(serverEntryList);
    }
    else
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Couldn't find server"), __TFUNCTION__);
    }
}

void ServerList::MarkServerFailed(const ServerEntry& failedServerEntry)
{
    ServerEntries failedServerEntries;
    failedServerEntries.push_back(failedServerEntry);
    MarkServersFailed(failedServerEntries);
}

// This function should not throw
ServerEntries ServerList::GetList()
{
    AutoMUTEX lock(m_mutex);

    // Load persistent list of servers from system (registry)

    ServerEntries systemServerEntryList;

    if (!IGNORE_SYSTEM_SERVER_LIST)
    {
        try
        {
            systemServerEntryList = GetListFromSystem();
        }
        catch (std::exception &ex)
        {
            my_print(NOT_SENSITIVE, false, string("Not using corrupt System Server List: ") + ex.what());
        }
    }

    // Add embedded list to system list.
    // Cases:
    // - This may be a new client run on a system with an existing registry entry; we want the new embedded values
    // - This may be the first run, in which case the system list is empty

    ServerEntries embeddedServerEntryList;
    try
    {
        embeddedServerEntryList = GetListFromEmbeddedValues();
        // Randomize this list for load-balancing
        random_shuffle(embeddedServerEntryList.begin(), embeddedServerEntryList.end());
    }
    catch (std::exception &ex)
    {
        my_print(NOT_SENSITIVE, false, string("Not using corrupt Embedded Server List: ") + ex.what());
        embeddedServerEntryList.clear();
    }

    for (ServerEntries::iterator embeddedServerEntry = embeddedServerEntryList.begin();
         embeddedServerEntry != embeddedServerEntryList.end(); ++embeddedServerEntry)
    {
        // Check if we already know about this server
        // We prioritize discovery information, so skip embedded entry entirely when already known
        bool alreadyKnown = false;
        for (ServerEntries::iterator systemServerEntry = systemServerEntryList.begin();
             systemServerEntry != systemServerEntryList.end(); ++systemServerEntry)
        {
            if (embeddedServerEntry->serverAddress == systemServerEntry->serverAddress)
            {
                alreadyKnown = true;

                // Special case: if the embedded server entry has new info that the
                // existing system entry does not, we know the embedded entry is actually newer
                if (embeddedServerEntry->sshObfuscatedKey.length() > 0 &&
                    systemServerEntry->sshObfuscatedKey.length() == 0)
                {
                    systemServerEntry->Copy(*embeddedServerEntry);
                }

                break;
            }
        }

        if (!alreadyKnown)
        {
            // Insert the new entry as the second entry (if there already is at least one),
            // so that the first entry can continue to be used if it is reachable
            systemServerEntryList.insert(systemServerEntryList.size() > 0 ?
                                            systemServerEntryList.begin() + 1 :
                                            systemServerEntryList.begin(),
                                         *embeddedServerEntry);
        }
    }

    // Write this out immediately, so the next time we'll get it from the system
    // (Also so MarkCurrentServerFailed reads the same list we're returning)
    WriteListToSystem(systemServerEntryList);

    // WriteListToSystem could truncate the list if it is too long to write to the registry.
    // Try to return what is stored in the system for consistency.
    try
    {
        return GetListFromSystem();
    }
    catch (std::exception &ex)
    {
        my_print(NOT_SENSITIVE, true, string("Just wrote a corrupt System Server List: ") + ex.what());
        return systemServerEntryList;
    }
}

string ServerList::GetListName() const
{
    return string(LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS) + m_name;
}

ServerEntries ServerList::GetListFromEmbeddedValues()
{
    return ParseServerEntries(EMBEDDED_SERVER_LIST);
}

ServerEntries ServerList::GetListFromSystem()
{
    return GetListFromSystem(GetListName().c_str());
}

ServerEntries ServerList::GetListFromSystem(const char* listName)
{
    string serverEntryListString;

    if (!ReadRegistryStringValue(
            listName, 
            serverEntryListString))
    {
        // If we're migrating from an old version, there's no m_name qualifier.
        if (!ReadRegistryStringValue(
                LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS, 
                serverEntryListString))
        {
            return ServerEntries();
        }
    }

    return ParseServerEntries(serverEntryListString.c_str());
}

// The errors below throw (preventing any Server connection from starting)
ServerEntries ServerList::ParseServerEntries(const char* serverEntryListString)
{
    ServerEntries serverEntryList;

    stringstream stream(serverEntryListString);
    string item;

    while (getline(stream, item, '\n'))
    {
        ServerEntry entry = ParseServerEntry(item);
        if (entry.webServerCertificate != "None")
        {
            serverEntryList.push_back(entry);
        }
    }

    return serverEntryList;
}

ServerEntry ServerList::ParseServerEntry(const string& serverEntry)
{
    string line = Dehexlify(serverEntry);

    ServerEntry entry;
    entry.FromString(line);

    return entry;
}

// NOTE: This function does not throw because we don't want a failure to prevent a connection attempt.
void ServerList::WriteListToSystem(const ServerEntries& serverEntryList)
{
    string encodedServerEntryList = EncodeServerEntries(serverEntryList);

    RegistryFailureReason reason = REGISTRY_FAILURE_NO_REASON;

    if (!WriteRegistryStringValue(
            GetListName().c_str(), 
            encodedServerEntryList, 
            reason))
    {
        if (REGISTRY_FAILURE_WRITE_TOO_LONG == reason)
        {
            int bisect = serverEntryList.size()/2;
            if (bisect > 1)
            {
                my_print(NOT_SENSITIVE, true, _T("%s: List is too long to write to registry, truncating"), __TFUNCTION__);
                ServerEntries truncatedServerEntryList(serverEntryList.begin(),
                                                       serverEntryList.begin() + bisect);
                WriteListToSystem(truncatedServerEntryList);
            }
            else
            {
                my_print(NOT_SENSITIVE, true,
                    _T("%s: List is still too long to write to registry, but there are only %ld entries"),
                    __TFUNCTION__, serverEntryList.size());
            }
        }
    }
}

string ServerList::EncodeServerEntries(const ServerEntries& serverEntryList)
{
    string encodedServerList;
    for (ServerEntryIterator it = serverEntryList.begin(); it != serverEntryList.end(); ++it)
    {
        string stringServerEntry = it->ToString();
        encodedServerList += Hexlify(
                                (const unsigned char*)stringServerEntry.c_str(),
                                stringServerEntry.length()) + "\n";
    }
    return encodedServerList;
}


/***********************************************
ServerEntry members
*/

ServerEntry::ServerEntry(
    const string& serverAddress, const string& region, int webServerPort,
    const string& webServerSecret, const string& webServerCertificate, 
    int sshPort, const string& sshUsername, const string& sshPassword, 
    const string& sshHostKey, int sshObfuscatedPort, 
    const string& sshObfuscatedKey,
    const string& meekObfuscatedKey, const int meekServerPort,
    const string& meekCookieEncryptionPublicKey,
    const string& meekFrontingDomain, const string& meekFrontingHost,
    const string& meekFrontingAddressesRegex,
    const vector<string>& meekFrontingAddresses,
    const vector<string>& capabilities)
{
    this->serverAddress = serverAddress;
    this->region = region;
    this->webServerPort = webServerPort;
    this->webServerSecret = webServerSecret;
    this->webServerCertificate = webServerCertificate;
    this->sshPort = sshPort;
    this->sshUsername = sshUsername;
    this->sshPassword = sshPassword;
    this->sshHostKey = sshHostKey;
    this->sshObfuscatedPort = sshObfuscatedPort;
    this->sshObfuscatedKey = sshObfuscatedKey;
    this->meekObfuscatedKey = meekObfuscatedKey;
    this->meekServerPort =  meekServerPort;
    this->meekCookieEncryptionPublicKey = meekCookieEncryptionPublicKey;
    this->meekFrontingDomain = meekFrontingDomain;
    this->meekFrontingHost = meekFrontingHost;
    this->meekFrontingAddressesRegex = meekFrontingAddressesRegex;
    this->meekFrontingAddresses = meekFrontingAddresses;

    this->capabilities = capabilities;
}

void ServerEntry::Copy(const ServerEntry& src)
{
    *this = src;
}

string ServerEntry::ToString() const
{
    stringstream ss;
    
    //
    // Legacy values are simply space-separated strings
    //

    ss << serverAddress << " ";
    ss << webServerPort << " ";
    ss << webServerSecret << " ";
    ss << webServerCertificate << " ";

    //
    // Extended values are JSON-encoded.
    //

    // Note: for legacy reasons, webServerPort is a string, not an int
    ostringstream webServerPortString;
    webServerPortString << webServerPort;

    Json::Value entry;
    
    entry["ipAddress"] = serverAddress;
    entry["region"] = region;
    entry["webServerPort"] = webServerPortString.str();
    entry["webServerCertificate"] = webServerCertificate;
    entry["webServerSecret"] = webServerSecret;
    entry["sshPort"] = sshPort;
    entry["sshUsername"] = sshUsername;
    entry["sshPassword"] = sshPassword;
    entry["sshHostKey"] = sshHostKey;
    entry["sshObfuscatedPort"] = sshObfuscatedPort;
    entry["sshObfuscatedKey"] = sshObfuscatedKey;
    entry["meekObfuscatedKey"] = meekObfuscatedKey;
    entry["meekServerPort"] = meekServerPort;
    entry["meekFrontingDomain"] = meekFrontingDomain;
    entry["meekFrontingHost"] = meekFrontingHost;
    entry["meekCookieEncryptionPublicKey"] = meekCookieEncryptionPublicKey;
    entry["meekFrontingAddressesRegex"] = meekFrontingAddressesRegex;

    Json::Value capabilities(Json::arrayValue);
    for (vector<string>::const_iterator i = this->capabilities.begin(); i != this->capabilities.end(); i++)
    {
        capabilities.append(*i);
    }
    entry["capabilities"] = capabilities;

    Json::Value meekFrontingAddresses(Json::arrayValue);
    for (vector<string>::const_iterator i = this->meekFrontingAddresses.begin(); i != this->meekFrontingAddresses.end(); i++)
    {
        meekFrontingAddresses.append(*i);
    }
    entry["meekFrontingAddresses"] = meekFrontingAddresses;

    Json::FastWriter jsonWriter;
    ss << jsonWriter.write(entry);

    return ss.str();
}

void ServerEntry::FromString(const string& str)
{
    stringstream lineStream(str);
    string lineItem;

    //
    // Legacy values are simply space-separated strings
    //

    if (!getline(lineStream, lineItem, ' '))
    {
        throw std::exception("Server Entries are corrupt: can't parse Server Address");
    }
    serverAddress = lineItem;

    if (!getline(lineStream, lineItem, ' '))
    {
        throw std::exception("Server Entries are corrupt: can't parse Web Server Port");
    }
    webServerPort = (int) strtol(lineItem.c_str(), NULL, 10);

    if (!getline(lineStream, lineItem, ' '))
    {
        throw std::exception("Server Entries are corrupt: can't parse Web Server Secret");
    }
    webServerSecret = lineItem;

    if (!getline(lineStream, lineItem, ' '))
    {
        throw std::exception("Server Entries are corrupt: can't parse Web Server Certificate");
    }
    webServerCertificate = lineItem;

    //
    // Extended values are JSON-encoded.
    //

    if (!getline(lineStream, lineItem, '\0'))
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Extended JSON values not present"), __TFUNCTION__);
        
        // Assumption: we're not reading into a ServerEntry struct that already
        // has values set. So we're relying on the default values being set by
        // the constructor.
        return;
    }

    Json::Value json_entry;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(lineItem, json_entry);
    if (!parsingSuccessful)
    {
        string fail = reader.getFormattedErrorMessages();
        my_print(NOT_SENSITIVE, false, _T("%s: Extended JSON parse failed: %S"), __TFUNCTION__, reader.getFormattedErrorMessages().c_str());
        throw std::exception("Server Entries are corrupt: can't parse JSON");
    }


    // At the time of introduction of the server capabilities feature
    // these are the default capabilities possessed by all servers.
    Json::Value defaultCapabilities(Json::arrayValue);
    defaultCapabilities.append("OSSH");
    defaultCapabilities.append("SSH");
    defaultCapabilities.append("VPN");
    defaultCapabilities.append("handshake");

    try
    {
        region = json_entry.get("region", "").asString();
        sshPort = json_entry.get("sshPort", 0).asInt();
        sshUsername = json_entry.get("sshUsername", "").asString();
        sshPassword = json_entry.get("sshPassword", "").asString();
        sshHostKey = json_entry.get("sshHostKey", "").asString();
        sshObfuscatedPort = json_entry.get("sshObfuscatedPort", 0).asInt();
        sshObfuscatedKey = json_entry.get("sshObfuscatedKey", "").asString();

        Json::Value capabilities;
        capabilities = json_entry.get("capabilities", defaultCapabilities);

        this->capabilities.clear();
        for (Json::ArrayIndex i = 0; i < capabilities.size(); i++)
        {
            string item = capabilities.get(i, "").asString();
            if (!item.empty())
            {
                this->capabilities.push_back(item);
            }
        }
        
        if (HasCapability("FRONTED-MEEK") || HasCapability("UNFRONTED-MEEK") || HasCapability("UNFRONTED-MEEK-HTTPS"))
        {
            meekServerPort = json_entry.get("meekServerPort", 0).asInt();
            meekObfuscatedKey = json_entry.get("meekObfuscatedKey", "").asString();
            meekCookieEncryptionPublicKey = json_entry.get("meekCookieEncryptionPublicKey", "").asString();
        }
        else
        {
            meekServerPort = -1;
            meekObfuscatedKey = "";
            meekCookieEncryptionPublicKey = "";
        }

        if (HasCapability("FRONTED-MEEK"))
        {
            meekFrontingDomain = json_entry.get("meekFrontingDomain", "").asString();
            meekFrontingHost  = json_entry.get("meekFrontingHost", "").asString();
            meekFrontingAddressesRegex = json_entry.get("meekFrontingAddressesRegex", "").asString();
            Json::Value meekFrontingAddresses;
            Json::Value emptyArray(Json::arrayValue);
            meekFrontingAddresses = json_entry.get("meekFrontingAddresses", emptyArray);
            this->meekFrontingAddresses.clear();
            for (Json::ArrayIndex i = 0; i < meekFrontingAddresses.size(); i++)
            {
                string item = meekFrontingAddresses.get(i, "").asString();
                if (!item.empty())
                {
                    this->meekFrontingAddresses.push_back(item);
                }
            }
        }
        else
        {
            meekFrontingDomain = "";
            meekFrontingHost  = "";
            meekFrontingAddressesRegex = "";
            meekFrontingAddresses.clear();
        }
    }
    catch (exception& e)
    {
        my_print(NOT_SENSITIVE, false, _T("%s: Extended JSON parse exception: %S"), __TFUNCTION__, e.what());
        throw std::exception("Server Entries are corrupt: parse JSON exception");
    }
}

bool ServerEntry::HasCapability(const string& capability) const
{
    for (size_t i = 0; i < this->capabilities.size(); i++)
    {
        if (this->capabilities[i] == capability)
        {
            return true;
        }
    }

    return false;
}

int ServerEntry::GetPreferredReachablityTestPort() const
{
    if (HasCapability("OSSH"))
    {
        return sshObfuscatedPort;
    }
    else if (HasCapability("SSH"))
    {
        return sshPort;
    }
    else if (HasCapability("handshake"))
    {
        return webServerPort;
    }

    return -1;
}
