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
#include "transport.h"


/******************************************************************************
 TransportBase
******************************************************************************/

TransportBase::TransportBase(ConnectionManager* manager)
    : m_manager(manager) 
{
}

bool TransportBase::Connect(const ServerEntry& serverEntry)
{
    if (!PreConnect())
        return false;

    if (!TransportConnect(serverEntry))
    {
        return false;
    }

    m_manager->SetState(GetConnectedState());

    return true;
}

/******************************************************************************
 VPNTransport
******************************************************************************/

void VPNTransport::WaitForDisconnect()
{
    m_manager->WaitForVPNConnectionStateToChangeFrom(VPN_CONNECTION_STATE_CONNECTED);
}

tstring VPNTransport::GetConnectFailedRequestPath() const 
{
    // The request line includes the last VPN error code.
    return m_manager->GetVPNFailedRequestPath();
}

tstring VPNTransport::GetConnectSuccessRequestPath() const 
{
    return m_manager->GetVPNConnectRequestPath();
}

void VPNTransport::Cleanup()
{
    m_manager->RemoveVPNConnection();
}

bool VPNTransport::PreConnect()
{
    // If the "Skip VPN" flag is set, the last time the user
    // connected with this server, VPN failed. So we don't
    // try VPN again.
    // Note: this flag is cleared whenever the first server
    // in the list changes, so VPN will be tried again.

    bool skipVPN = manager->GetSkipVPN();

    m_manager->SetCurrentConnectionSkippedVPN(skipVPN);

    // NOTE: IGNORE_VPN_RELAY is for automated testing only

    if (IGNORE_VPN_RELAY || skipVPN || UserSkipVPN())
    {
        return false;
    }

    if (!m_manager->CurrentServerVPNCapable())
    {
        return false;
    }

    //
    // Minimum version check for VPN
    // - L2TP/IPSec/PSK not supported on Windows 2000
    // - Throws to try next server -- an assumption here is we'll always try SSH next
    //
    
    OSVERSIONINFO versionInfo;
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    if (!GetVersionEx(&versionInfo) ||
            versionInfo.dwMajorVersion < 5 ||
            (versionInfo.dwMajorVersion == 5 && versionInfo.dwMinorVersion == 0))
    {
        my_print(false, _T("VPN requires Windows XP or greater"));
        return false;
    }

    return true;
};

bool VPNTransport::TransportConnect(const ServerEntry& serverEntry)
{
    //
    // Check VPN services and fix if required/possible
    //
    
    // Note: we proceed even if the call fails. Testing is inconsistent -- don't
    // always need all tweaks to connect.
    TweakVPN();

    //
    // Start VPN connection
    //
    
    m_manager->VPNEstablish();

    //
    // Monitor VPN connection and wait for CONNECTED or FAILED
    //
    
    m_manager->WaitForVPNConnectionStateToChangeFrom(VPN_CONNECTION_STATE_STARTING);
    
    if (VPN_CONNECTION_STATE_CONNECTED != m_manager->GetVPNConnectionState())
    {
        // Note: WaitForVPNConnectionStateToChangeFrom throws Abort if user
        // cancelled, so if we're here it's a FAILED case.
        return false;
    }

    //
    // Patch DNS bug on Windowx XP; and flush DNS
    // to ensure domains are resolved with VPN's DNS server
    //
    
    // Note: we proceed even if the call fails. This means some domains
    // may not resolve properly.
    TweakDNS();
    

    return true;
}

/******************************************************************************
 SSHTransportBase
******************************************************************************/

void SSHTransportBase::WaitForDisconnect()
{
    //
    // Wait for SSH connection to stop (or fail)
    //

    // Note: doesn't throw abort on user cancel, but it all works out the same
    m_manager->SSHWaitAndDisconnect();
}

void SSHTransportBase::Cleanup()
{
    m_manager->SSHDisconnect();
}

tstring SSHTransportBase::GetConnectFailedRequestPath() const 
{
    return m_manager->GetSSHFailedRequestPath(GetSSHType());
}

tstring SSHTransportBase::GetConnectSuccessRequestPath() const 
{
    return m_manager->GetSSHConnectRequestPath(GetSSHType());
}

bool SSHTransportBase::PreConnect() 
{
    if (!manager->CurrentServerSSHCapable())
    {
        return false;
    }

    return true;
};

bool SSHTransportBase::TransportConnect(const ServerEntry& serverEntry)
{
    if (!manager->SSHConnect(connectType) || !manager->SSHWaitForConnected())
    {
        // Explicit disconnect cleanup before next attempt (SSH object cleans
        // up automatically, but that results in confusing log output).

        manager->SSHDisconnect();
        return false;
    }

    return true;
}
