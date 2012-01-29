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
- break out transports into separate files
- merge transport and connection classes
- remove transport-specific logic from connectionmanager
- make vpnlist more generic (maybe just needs rename)
- make previous-transport-failure records more generic
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

    // Call to create the connection
    bool Connect(const ServerEntry& serverEntry);

    // Call after connection to wait for disconnection.
    // Will also disconnect when the abort flag is set to true.
    virtual void WaitForDisconnect() = 0;

    // Do any necessary final cleanup. 
    // If restartImminent is true, then the same transport will reconnect
    // momentarily, which may affect the cleanup steps.
    virtual bool Cleanup(bool restartImminent) = 0;

    // Exception classes to help with the control flow
    // Indicates that this transport was not successful
    class TransportFailed { };
    // Indicates that a connection abort was requested
    class Abort { };

protected:
    virtual bool TransportConnect(const ServerEntry& serverEntry) = 0;

protected:
    ConnectionManager* m_manager;
};



// Base class for the SSH transports
class SSHTransportBase: public TransportBase
{
public:
    virtual ConnectionManagerState GetConnectedState() const { return CONNECTION_MANAGER_STATE_CONNECTED_SSH; }
    virtual void WaitForDisconnect();
    virtual void Cleanup();
    virtual tstring GetConnectFailedRequestPath() const;
    virtual tstring GetConnectSuccessRequestPath() const; 

protected:
    virtual bool TransportConnect(const ServerEntry& serverEntry);

    virtual int GetSSHType() const = 0;
};

// Standard SSH
class SSHTransport: public SSHTransportBase
{
public:
    tstring GetTransportName() const { return _T("SSH"); }

protected:
    int GetSSHType() const { return SSH_CONNECT_STANDARD; }
};

// Obfuscated SSH
class OSSHTransport: public SSHTransportBase
{
public:
    tstring GetTransportName() const { return _T("OSSH"); }

protected:
    int GetSSHType() const { return SSH_CONNECT_OBFUSCATED; }
};
