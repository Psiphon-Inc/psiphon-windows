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
#include "psiclient.h"
#include "serverlist.h"
#include "embeddedvalues.h"
#include "config.h"
#include "utilities.h"
#include <algorithm>
#include <sstream>

ServerList::ServerList()
{
    m_mutex = CreateMutex(NULL, FALSE, 0);
}

ServerList::~ServerList()
{
    CloseHandle(m_mutex);
}

void ServerList::AddEntriesToList(const vector<string>& newServerEntryList)
{
    AutoMUTEX lock(m_mutex, __TFUNCTION__);

    if (newServerEntryList.size() < 1)
    {
        return;
    }

    ServerEntries oldServerEntryList = GetList();
    for (vector<string>::const_iterator newServerEntryString = newServerEntryList.begin();
         newServerEntryString != newServerEntryList.end(); ++newServerEntryString)
    {
        ServerEntry newServerEntry = ParseServerEntry(*newServerEntryString);

        // Check if we already know about this server
        bool alreadyKnown = false;
        for (ServerEntries::iterator oldServerEntry = oldServerEntryList.begin();
             oldServerEntry != oldServerEntryList.end(); ++oldServerEntry)
        {
            if (newServerEntry.serverAddress == oldServerEntry->serverAddress)
            {
                alreadyKnown = true;
                // NOTE: We always update the values for known servers, because we trust the
                //       discovery mechanisms
                oldServerEntry->webServerPort = newServerEntry.webServerPort;
                oldServerEntry->webServerSecret = newServerEntry.webServerSecret;
                oldServerEntry->webServerCertificate = newServerEntry.webServerCertificate;
                break;
            }
        }

        if (!alreadyKnown)
        {
            // Insert the new entry as the second entry, so that the first entry can continue
            // to be used if it is reachable
            oldServerEntryList.insert(oldServerEntryList.begin() + 1, newServerEntry);
        }
    }

    WriteListToSystem(oldServerEntryList);
}

void ServerList::MarkCurrentServerFailed()
{
    AutoMUTEX lock(m_mutex, __TFUNCTION__);

    ServerEntries serverEntryList = GetList();
    if (serverEntryList.size() > 1)
    {
        // Move the first server to the end of the list
        serverEntryList.push_back(serverEntryList[0]);
        serverEntryList.erase(serverEntryList.begin());
        WriteListToSystem(serverEntryList);
    }
}

ServerEntry ServerList::GetNextServer()
{
    AutoMUTEX lock(m_mutex, __TFUNCTION__);

    ServerEntries serverEntryList = GetList();
    if (serverEntryList.size() < 1)
    {
        throw std::exception("No servers found.  This application is possibly corrupt.");
    }

    // The client always tries the first entry in the list.
    // The list will be rearranged elsewhere, such as when a server has failed,
    // or when new servers are discovered.
    return serverEntryList[0];
}

ServerEntries ServerList::GetList()
{
    AutoMUTEX lock(m_mutex, __TFUNCTION__);

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
            my_print(false, string("Not using corrupt System Server List: ") + ex.what());
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
        string message = string("Corrupt Embedded Server List: ") + ex.what();
        throw std::exception(message.c_str());
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

    return systemServerEntryList;
}

ServerEntries ServerList::GetListFromEmbeddedValues()
{
    return ParseServerEntries(EMBEDDED_SERVER_LIST);
}

ServerEntries ServerList::GetListFromSystem()
{
    string serverEntryListString;

    if (!ReadRegistryStringValue(LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS, serverEntryListString))
    {
         return ServerEntries();
    }

    return ParseServerEntries(serverEntryListString.c_str());
}

// Adapted from here:
// http://stackoverflow.com/questions/3381614/c-convert-string-to-hexadecimal-and-vice-versa
string Hexlify(const string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();

    string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

string Dehexlify(const string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();
    if (len & 1)
    {
        throw std::invalid_argument("Dehexlify: odd length");
    }

    string output;
    output.reserve(len / 2);
    for (size_t i = 0; i < len; i += 2)
    {
        char a = toupper(input[i]);
        const char* p = std::lower_bound(lut, lut + 16, a);
        if (*p != a)
        {
            throw std::invalid_argument("Dehexlify: not a hex digit");
        }

        char b = toupper(input[i + 1]);
        const char* q = std::lower_bound(lut, lut + 16, b);
        if (*q != b)
        {
            throw std::invalid_argument("Dehexlify: not a hex digit");
        }

        output.push_back(((p - lut) << 4) | (q - lut));
    }

    return output;
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

    stringstream lineStream(line);
    string lineItem;
    ServerEntry entry;
        
    if (!getline(lineStream, lineItem, ' '))
    {
        throw std::exception("Server Entries are corrupt: can't parse Server Address");
    }
    entry.serverAddress = lineItem;

    if (!getline(lineStream, lineItem, ' '))
    {
        throw std::exception("Server Entries are corrupt: can't parse Web Server Port");
    }
    entry.webServerPort = atoi(lineItem.c_str());

    if (!getline(lineStream, lineItem, ' '))
    {
        throw std::exception("Server Entries are corrupt: can't parse Web Server Secret");
    }
    entry.webServerSecret = lineItem;

    if (!getline(lineStream, lineItem, ' '))
    {
        throw std::exception("Server Entries are corrupt: can't parse Web Server Certificate");
    }
    entry.webServerCertificate = lineItem;

    if (!getline(lineStream, lineItem, ' '))
    {
        my_print(true, _T("%s: SSH Port not present", __TFUNCTION__));
        entry.sshPort = 0;
    }
    entry.sshPort = atoi(lineItem.c_str());;

    if (!getline(lineStream, lineItem, ' '))
    {
        my_print(true, _T("%s: SSH Username not present", __TFUNCTION__));
        entry.sshUsername = "";
    }
    entry.sshUsername = lineItem;

    if (!getline(lineStream, lineItem, ' '))
    {
        my_print(true, _T("%s: SSH Password not present", __TFUNCTION__));
        entry.sshPassword = "";
    }
    entry.sshPassword = lineItem;

    if (!getline(lineStream, lineItem, ' '))
    {
        my_print(true, _T("%s: SSH Host Key not present", __TFUNCTION__));
        entry.sshHostKey = "";
    }
    entry.sshHostKey = lineItem;

    if (!getline(lineStream, lineItem, ' '))
    {
        my_print(true, _T("%s: SSH Obfuscated Port not present", __TFUNCTION__));
        entry.sshObfuscatedPort = 0;
    }
    entry.sshObfuscatedPort = atoi(lineItem.c_str());;

    if (!getline(lineStream, lineItem, ' '))
    {
        my_print(true, _T("%s: SSH Obfuscated Key not present", __TFUNCTION__));
        entry.sshObfuscatedKey = "";
    }
    entry.sshObfuscatedKey = lineItem;

    return entry;
}

// NOTE: This function does not throw because we don't want a failure to prevent a connection attempt.
void ServerList::WriteListToSystem(const ServerEntries& serverEntryList)
{
    string encodedServerEntryList = EncodeServerEntries(serverEntryList);

    WriteRegistryStringValue(LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS, encodedServerEntryList);
}

string ServerList::EncodeServerEntries(const ServerEntries& serverEntryList)
{
    string encodedServerList;
    for (ServerEntryIterator it = serverEntryList.begin(); it != serverEntryList.end(); ++it)
    {
        stringstream port;
        port << it->webServerPort;
        string serverEntry = it->serverAddress + " " + port.str() + " " + it->webServerSecret + " " + it->webServerCertificate;
        encodedServerList += Hexlify(serverEntry) + "\n";
    }
    return encodedServerList;
}

