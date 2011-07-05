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
#include "vpnlist.h"
#include "embeddedvalues.h"
#include "config.h"
#include <algorithm>
#include <sstream>

VPNList::VPNList(void)
{
}

VPNList::~VPNList(void)
{
}

void VPNList::AddEntriesToList(const vector<string>& newServerEntryList)
{
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
            // Insert the new entry below the current (first) entry, but not at the bottom,
            // because that may contain a previously failed entry
            oldServerEntryList.insert(oldServerEntryList.begin() + 1, newServerEntry);
        }
    }

    WriteListToSystem(oldServerEntryList);
}

void VPNList::MarkCurrentServerFailed(void)
{
    ServerEntries serverEntryList = GetList();
    if (serverEntryList.size() > 1)
    {
        // Move the first server to the end of the list
        serverEntryList.push_back(serverEntryList[0]);
        serverEntryList.erase(serverEntryList.begin());
        WriteListToSystem(serverEntryList);
    }
}

ServerEntry VPNList::GetNextServer(void)
{
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

ServerEntries VPNList::GetList(void)
{
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
            // Insert the new entry at the top
            systemServerEntryList.insert(systemServerEntryList.begin(), *embeddedServerEntry);
        }
    }

    // Write this out immediately, so the next time we'll get it from the system
    // (Also so MarkCurrentServerFailed reads the same list we're returning)
    WriteListToSystem(systemServerEntryList);

    return systemServerEntryList;
}

ServerEntries VPNList::GetListFromEmbeddedValues(void)
{
    return ParseServerEntries(EMBEDDED_SERVER_LIST);
}

ServerEntries VPNList::GetListFromSystem(void)
{
    HKEY key = 0;
    DWORD disposition = 0;
    LONG returnCode = RegCreateKeyEx(HKEY_CURRENT_USER, LOCAL_SETTINGS_REGISTRY_KEY, 0, 0, 0, KEY_READ, 0, &key, &disposition);
    if (ERROR_SUCCESS != returnCode)
    {
        std::stringstream s;
        s << "Create Registry Key failed (" << returnCode << ")";
        throw std::exception(s.str().c_str());
    }

    DWORD bufferLength = 1;
    char *buffer = (char *)malloc(bufferLength * sizeof(char));
    if (!buffer)
    {
        RegCloseKey(key);
        throw std::exception("GetListFromSystem: Error allocating memory");
    }

    // Using the ANSI version explicitly.
    returnCode = RegQueryValueExA(key, LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS, 0, 0, (LPBYTE)buffer, &bufferLength);
    if (ERROR_MORE_DATA == returnCode)
    {
        // We must ensure that the string is null terminated, as per MSDN
        char *newBuffer = (char *)realloc(buffer, bufferLength + 1);
        if (!newBuffer)
        {
            free(buffer);
            RegCloseKey(key);
            throw std::exception("GetListFromSystem: Error reallocating memory");
        }
        buffer = newBuffer;
        buffer[bufferLength - 1] = '\0';
        returnCode = RegQueryValueExA(key, LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS, 0, 0, (LPBYTE)buffer, &bufferLength);
    }

    string serverEntryListString(buffer);
    free(buffer);
    RegCloseKey(key);

    if (ERROR_FILE_NOT_FOUND == returnCode)
    {
        return ServerEntries();
    }
    else if (ERROR_SUCCESS != returnCode)
    {
        std::stringstream s;
        s << "Query Registry Value failed (" << returnCode << ")";
        throw std::exception(s.str().c_str());
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

// The errors below throw (preventing any VPN connection from starting)
ServerEntries VPNList::ParseServerEntries(const char* serverEntryListString)
{
    ServerEntries serverEntryList;

    stringstream stream(serverEntryListString);
    string item;

    while (getline(stream, item, '\n'))
    {
        serverEntryList.push_back(ParseServerEntry(item));
    }

    return serverEntryList;
}

ServerEntry VPNList::ParseServerEntry(const string& serverEntry)
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

    return entry;
}

// NOTE: This function does not throw because we don't want a failure to prevent a connection attempt.
void VPNList::WriteListToSystem(const ServerEntries& serverEntryList)
{
    HKEY key = 0;
    DWORD disposition = 0;
    LONG returnCode = RegCreateKeyEx(HKEY_CURRENT_USER, LOCAL_SETTINGS_REGISTRY_KEY, 0, 0, 0, KEY_WRITE, 0, &key, &disposition);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("Create Registry Key failed (%d)"), returnCode);
        return;
    }

    string encodedServerEntryList = EncodeServerEntries(serverEntryList);
    // REG_MULTI_SZ needs two terminating null characters.  We're using REG_SZ right now, but I'm leaving this in anyways.
    int bufferLength = encodedServerEntryList.length() + 2;
    char *buffer = (char *)malloc(bufferLength * sizeof(char));
    if (!buffer)
    {
        my_print(false, _T("WriteListToSystem: Error allocating memory"));
        RegCloseKey(key);
        return;
    }
    sprintf_s(buffer, bufferLength, encodedServerEntryList.c_str());
    buffer[bufferLength - 1] = '\0';
    buffer[bufferLength - 2] = '\0';

    // Using the ANSI version explicitly.
    returnCode = RegSetValueExA(key, LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS, 0, REG_SZ, (PBYTE)buffer, bufferLength);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("Set Registry Value failed (%d)"), returnCode);
    }
    free(buffer);

    RegCloseKey(key);
}

string VPNList::EncodeServerEntries(const ServerEntries& serverEntryList)
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
