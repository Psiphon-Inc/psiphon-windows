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
    ServerEntry() : webServerPort(0), sshPort(0), sshObfuscatedPort(0) {}
    ServerEntry(const ServerEntry& src) { Copy(src); }
    ServerEntry(
        const string& serverAddress, int webServerPort, 
        const string& webServerSecret, const string& webServerCertificate, 
        int sshPort, const string& sshUsername, const string& sshPassword, 
        const string& sshHostKey, int sshObfuscatedPort, 
        const string& sshObfuscatedKey,
        const vector<string>& capabilities);
    void Copy(const ServerEntry& src);

    string ToString() const;
    void FromString(const string& str);

    bool HasCapability(const string& capability) const;

    string serverAddress;
    int webServerPort;
    string webServerSecret;
    string webServerCertificate;
    int sshPort;
    string sshUsername;
    string sshPassword;
    string sshHostKey;
    int sshObfuscatedPort;
    string sshObfuscatedKey;
    vector<string> capabilities;
};

typedef vector<ServerEntry> ServerEntries;
typedef ServerEntries::const_iterator ServerEntryIterator;

class ServerList
{
public:
    ServerList();
    virtual ~ServerList();

    void MarkServerFailed(const string& serverAddress);
    ServerEntry GetNextServer();
    ServerEntries GetList();

    // serverEntry is optional. It is an extra server entry that should be
    // stored. Typically this is the current server with additional info.
    void AddEntriesToList(
        const vector<string>& newServerEntryList, 
        const ServerEntry* serverEntry);

    void MoveEntriesToFront(
        const ServerEntries& entries);

private:
    ServerEntries GetListFromEmbeddedValues();
    ServerEntries GetListFromSystem();
    ServerEntries ParseServerEntries(const char* serverEntryListString);
    ServerEntry ParseServerEntry(const string& serverEntry);
    void WriteListToSystem(const ServerEntries& serverEntryList);
    string EncodeServerEntries(const ServerEntries& serverEntryList);

    HANDLE m_mutex;
};
