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
- Make behaviour consistent when user cancels connect. 
  Right now it might throw abort or might exit cleanly (SSH for sure).
- The use of hInst in SSHTransport is probably too much coupling.
*/

#pragma once

class SessionInfo;
class ITransport;


// This interface must be implemented by the class that manages the transports 
// (i.e., ConnectionManager). Using this will help us keep track of which 
// functions in ConnectionManager are depended on by the transports, and are
// available to new transports.
class ITransportManager
{
public:
    virtual const bool& GetUserSignalledStop(bool throwIfTrue) = 0;
    virtual bool SendStatusMessage(
            ITransport* transport,
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

    // Every implementing class must have a static function with this signature:
    //static void GetFactory(tstring& o_transportName, TransportFactory& o_transportFactory);

    // Only valid when connected
    virtual tstring GetSessionID(SessionInfo sessionInfo) const = 0;

    virtual tstring GetLastTransportError() const = 0;

    // Call to create the connection.
    // A failed attempt must clean itself up as needed.
    // May throw TransportFailed or Abort
    // Subclasses must not override.
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

