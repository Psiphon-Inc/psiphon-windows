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

using namespace std;

struct ServerEntry
{
    ServerEntry() {}
    ServerEntry(const string& serverAddress_, int webServerPort_, const string& webServerSecret_, const string& webServerCertificate_) :
        serverAddress(serverAddress_), webServerPort(webServerPort_), webServerSecret(webServerSecret_) , webServerCertificate(webServerCertificate_) {}

    string serverAddress;
    int webServerPort;
    string webServerSecret;
    string webServerCertificate;
};

typedef vector<ServerEntry> ServerEntries;
typedef ServerEntries::const_iterator ServerEntryIterator;

class ServerList
{
public:
    ServerList();
    virtual ~ServerList();
    void AddEntriesToList(const vector<string>& newServerEntryList);
    void MarkCurrentServerFailed();
    ServerEntry GetNextServer();
    ServerEntries GetList();

private:
    ServerEntries GetListFromEmbeddedValues();
    ServerEntries GetListFromSystem();
    ServerEntries ParseServerEntries(const char* serverEntryListString);
    ServerEntry ParseServerEntry(const string& serverEntry);
    void WriteListToSystem(const ServerEntries& serverEntryList);
    string EncodeServerEntries(const ServerEntries& serverEntryList);

    HANDLE m_mutex;
};
