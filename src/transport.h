/*
 * Copyright (c) 2012, Psiphon Inc.
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

/*
NOTES
- make vpnlist more generic (maybe just needs rename)
- make previous-transport-failure records more generic
- make connection loop transport-agnostic
    - will need to change VPNlist and logic
*/

#pragma once

class SessionInfo;


// This interface must be implemented by the class that manages the transports 
// (i.e., ConnectionManager). Using this will help us keep track of which 
// functions in ConnectionManager are depended on by the transports, and are
// available to new transports.
class ITransportManager
{
public:
    virtual const bool& GetUserSignalledStop(bool throwIfTrue) = 0;
    virtual bool SendStatusMessage(
            bool connected,
            const map<string, int>& pageViewEntries,
            const map<string, int>& httpsRequestEntries,
            unsigned long long bytesTransferred) = 0;
};


// All transport implementations must implement this interface
class ITransport
{
public:
    ITransport(ITransportManager* manager);

    virtual tstring GetTransportName() const = 0;

    // Only valid when connected
    virtual tstring GetSessionID(SessionInfo sessionInfo) const = 0;

    virtual tstring GetLastTransportError() const = 0;

    // Call to create the connection.
    // A failed attempt must clean itself up as needed.
    // May throw TransportFailed or Abort
    void Connect(SessionInfo sessionInfo);

    // Call after connection to wait for disconnection.
    // Must clean itself up as needed.
    // May throw TransportFailed or Abort
    virtual void WaitForDisconnect() = 0;

    // Do any necessary final cleanup. 
    // Must be safe to call even if a connection was never established.
    virtual bool Cleanup() = 0;

    //
    // Exception classes
    //
    // Indicates that this transport was not successful
    class TransportFailed { };
    // Indicates a fatal system error
    class Error { };

protected:
    // May throw TransportFailed or Abort
    virtual void TransportConnect(const SessionInfo& sessionInfo) = 0;

protected:
    ITransportManager* m_manager;
};

typedef ITransport* (*TransportAllocator)(ITransportManager*);
class TransportFactory
{
public:
    static int Register(tstring transportName, TransportAllocator transportAllocator)
    {
        TransportFactory::m_registeredTransports[transportName] = transportAllocator;
        // The return value is essentially meaningless, but some return value 
        // is needed, so that an assignment can be done, to avoid an error 
        // that occurs otherwise when calling this with no scope.
        return TransportFactory::m_registeredTransports.size();
    }

    // TODO: Is this function only useful for testing? Should it be removed?
    static ITransport* New(tstring transportName, ITransportManager* manager)
    {
        return m_registeredTransports[transportName](manager);
    }

    static void NewAll(vector<ITransport*>& all_transports, ITransportManager* manager)
    {
        all_transports.clear();
        map<tstring, TransportAllocator>::const_iterator it;
        for (it = m_registeredTransports.begin(); it != m_registeredTransports.end(); it++)
        {
            all_transports.push_back(it->second(manager));
        }
    }

    static map<tstring, TransportAllocator> m_registeredTransports;
};
map<tstring, TransportAllocator> TransportFactory::m_registeredTransports;

