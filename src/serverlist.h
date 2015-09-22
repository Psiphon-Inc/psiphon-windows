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
        const vector<string>& capabilities);
    void Copy(const ServerEntry& src);

    string ToString() const;
    void FromString(const string& str);

    bool HasCapability(const string& capability) const;

    // returns -1 if there's no port
    int GetPreferredReachablityTestPort() const;

    string serverAddress;
    string region;
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
    string meekObfuscatedKey;
    int meekServerPort;
    string meekCookieEncryptionPublicKey;
    string meekFrontingDomain;
    string meekFrontingHost;
    string meekFrontingAddressesRegex;
    vector<string> meekFrontingAddresses;
};

typedef vector<ServerEntry> ServerEntries;
typedef ServerEntries::const_iterator ServerEntryIterator;

class ServerList
{
public:
    ServerList(LPCSTR listName);
    virtual ~ServerList();

    ServerEntries GetList();

    // serverEntry is optional. It is an extra server entry that should be
    // stored. Typically this is the current server with additional info.
    // Returns the number of new entries added.
    size_t AddEntriesToList(
        const vector<string>& newServerEntryList, 
        const ServerEntry* serverEntry);

    void MarkServersFailed(const ServerEntries& failedServerEntries);
    void MarkServerFailed(const ServerEntry& failedServerEntry);

    // Setting `veryFront` to true will force entries to go to the actual head
    // of the list, instead of just near it. Use carefully -- it can break server affinity.
    void MoveEntriesToFront(const ServerEntries& entries, bool veryFront=false);
    void MoveEntryToFront(const ServerEntry& serverEntry, bool veryFront=false);

    static ServerEntries GetListFromSystem(const char* listName);
    static string EncodeServerEntries(const ServerEntries& serverEntryList);

private:
    string GetListName() const;
    ServerEntries GetListFromEmbeddedValues();
    ServerEntries GetListFromSystem();
    static ServerEntries ParseServerEntries(const char* serverEntryListString);
    static ServerEntry ParseServerEntry(const string& serverEntry);
    void WriteListToSystem(const ServerEntries& serverEntryList);

    HANDLE m_mutex;
    string m_name;
};
