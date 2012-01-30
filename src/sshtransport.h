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

#pragma once

//
// Base class for the SSH transports
//
class SSHTransportBase: public TransportBase
{
public:
    SSHTransportBase(ConnectionManager* manager); 
    virtual ~SSHTransportBase();

    virtual tstring GetTransportName() const = 0;
    virtual tstring GetSessionID(SessionInfo sessionInfo) const;
    virtual tstring GetLastTransportError() const;

    virtual void WaitForDisconnect();
    virtual bool Cleanup(bool restartImminent);

protected:
    virtual void TransportConnect(const SessionInfo& sessionInfo);

    bool ServerSSHCapable(const SessionInfo& sessionInfo);
};


//
// Standard SSH
//
class SSHTransport: public SSHTransportBase
{
public:
    SSHTransport(ConnectionManager* manager); 
    virtual ~SSHTransport();

    tstring GetTransportName() const;
};


//
// Obfuscated SSH
//
class OSSHTransport: public SSHTransportBase
{
public:
    OSSHTransport(ConnectionManager* manager); 
    virtual ~OSSHTransport();

    tstring GetTransportName() const { return _T("OSSH"); }
};
