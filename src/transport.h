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
- modify connection loop to make sure API is okay
- break out transports into separate files
- merge transport and connection classes
- remove transport-specific logic from connectionmanager
- make vpnlist more generic (maybe just needs rename)
- make previous-transport-failure records more generic
- make connection loop transport-agnostic
    - will need to change VPNlist and logic
*/

#pragma once

#include "connectionmanager.h"


// All transport implementations should inherit this class. 
// It has many pure virtual members that must be implemented by subclasses.
class TransportBase
{
public:
    TransportBase(ConnectionManager* manager);

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

    // Exception classes to help with the control flow
    // Indicates that this transport was not successful
    class TransportFailed { };
    // Indicates that a connection abort was requested (i.e., by the user).
    class Abort { };

protected:
    // May throw TransportFailed or Abort
    virtual void TransportConnect(const SessionInfo& sessionInfo) = 0;

protected:
    ConnectionManager* m_manager;
};

typedef TransportBase* (*TransportAllocator)(ConnectionManager*);
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
    static TransportBase* New(tstring transportName, ConnectionManager* manager)
    {
        return m_registeredTransports[transportName](manager);
    }

    static void NewAll(vector<TransportBase*>& all_transports, ConnectionManager* manager)
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

