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

#include "stdafx.h"
#include "transport.h"
#include "sshtransport.h"


/******************************************************************************
 SSHTransportBase
******************************************************************************/

SSHTransportBase::SSHTransportBase(ConnectionManager* manager)
    : TransportBase(manager)
{
}

SSHTransportBase::~SSHTransportBase()
{
}

tstring SSHTransportBase::GetSessionID(SessionInfo sessionInfo) const
{
    return NarrowToTString(sessionInfo.GetSSHSessionID());
}

tstring SSHTransportBase::GetLastTransportError() const
{
    return _T("0");
}

void SSHTransportBase::WaitForDisconnect()
{
    //
    // Wait for SSH connection to stop (or fail)
    //

    // Note: doesn't throw abort on user cancel, but it all works out the same
    m_manager->SSHWaitAndDisconnect();
}

bool SSHTransportBase::Cleanup(bool restartImminent)
{
    m_manager->SSHDisconnect();
}

void SSHTransportBase::TransportConnect(const SessionInfo& sessionInfo)
{
    if (!ServerSSHCapable(sessionInfo))
    {
        throw TransportFailed();
    }

    if (!manager->SSHConnect(connectType) || !manager->SSHWaitForConnected())
    {
        // Explicit disconnect cleanup before next attempt (SSH object cleans
        // up automatically, but that results in confusing log output).

        manager->SSHDisconnect();
        return false;
    }
}

bool SSHTransportBase::ServerSSHCapable(const SessionInfo& sessionInfo)
{
    return sessionInfo.GetSSHHostKey().length() > 0;
}



/******************************************************************************
 SSHTransport
******************************************************************************/

static const TCHAR* SSH_TRANSPORT_NAME = _T("SSH");

// Set up the registration of this type
static TransportBase* NewSSH(ConnectionManager* manager)
{
    return new SSHTransport(manager);
}
static int _stub = TransportFactory::Register(SSH_TRANSPORT_NAME, &NewSSH);


SSHTransport::SSHTransport(ConnectionManager* manager)
    : SSHTransportBase(manager)
{
}

SSHTransport::~SSHTransport()
{
}

tstring SSHTransport::GetTransportName() const 
{ 
    return SSH_TRANSPORT_NAME; 
}


/******************************************************************************
 OSSHTransport
******************************************************************************/

static const TCHAR* OSSH_TRANSPORT_NAME = _T("OSSH");

// Set up the registration of this type
static TransportBase* NewOSSH(ConnectionManager* manager)
{
    return new OSSHTransport(manager);
}
static int _stub = TransportFactory::Register(OSSH_TRANSPORT_NAME, &NewOSSH);


OSSHTransport::OSSHTransport(ConnectionManager* manager)
    : SSHTransportBase(manager)
{
}

OSSHTransport::~OSSHTransport()
{
}

tstring OSSHTransport::GetTransportName() const 
{ 
    return OSSH_TRANSPORT_NAME; 
}
