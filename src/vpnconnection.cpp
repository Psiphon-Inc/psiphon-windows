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
#include "config.h"
#include "vpnmanager.h"
#include "vpnconnection.h"
#include "psiclient.h"
#include "ras.h"
#include "raserror.h"


void CALLBACK RasDialCallback(
    DWORD userData,
    DWORD,
    HRASCONN rasConnection,
    UINT,
    RASCONNSTATE rasConnState,
    DWORD dwError,
    DWORD)
{
    VPNConnection* vpnConnection = (VPNConnection*)userData;

    my_print(true, _T("RasDialCallback (%x %d)"), rasConnState, dwError);
    if (0 != dwError)
    {
        my_print(false, _T("Connection failed (%d)"), dwError);
        vpnConnection->SetState(VPN_CONNECTION_STATE_FAILED);
        vpnConnection->SetLastVPNErrorCode(dwError);
    }
    else if (RASCS_Connected == rasConnState)
    {
        // Set up a disconnection notification event
        AutoHANDLE rasEvent = CreateEvent(0, FALSE, FALSE, 0);
        DWORD returnCode = RasConnectionNotification(rasConnection, rasEvent, RASCN_Disconnection);
        if (ERROR_SUCCESS != returnCode)
        {
            my_print(false, _T("RasConnectionNotification failed (%d)"), returnCode);
            return;
        }

        my_print(false, _T("Successfully connected."));

        vpnConnection->SetState(VPN_CONNECTION_STATE_CONNECTED);

        if (WAIT_FAILED == WaitForSingleObject(rasEvent, INFINITE))
        {
            my_print(false, _T("WaitForSingleObject failed (%d)"), GetLastError());
            // Fall through to VPN_CONNECTION_STATE_STOPPED.
            // Otherwise we'd be stuck in a connected state.
        }

        my_print(false, _T("Disconnected."));
        vpnConnection->SetState(VPN_CONNECTION_STATE_STOPPED);
    }
    else
    {
        if (rasConnState == 0)
        {
            my_print(false, _T("Connecting..."));
        }
        my_print(true, _T("Establishing connection... (%x)"), rasConnState);
        vpnConnection->SetState(VPN_CONNECTION_STATE_STARTING);
    }
}

VPNConnection::VPNConnection(void) :
    m_state(VPN_CONNECTION_STATE_STOPPED),
    m_rasConnection(0),
    m_suspendTeardownForUpgrade(false),
    m_lastVPNErrorCode(0)
{
    m_stateChangeEvent = CreateEvent(NULL, FALSE, FALSE, 0);
}

VPNConnection::~VPNConnection(void)
{
    Remove();
    CloseHandle(m_stateChangeEvent);
}

void VPNConnection::SetState(VPNConnectionState newState)
{
    // No lock:
    // "Simple reads and writes to properly-aligned 32-bit variables are atomic operations."
    // http://msdn.microsoft.com/en-us/library/ms684122%28v=vs.85%29.aspx
    m_state = newState;
    SetEvent(m_stateChangeEvent);
}

VPNConnectionState VPNConnection::GetState(void)
{
    return m_state;
}

tstring VPNConnection::GetPPPIPAddress(void)
{
    tstring IPAddress;

    if (m_rasConnection && VPN_CONNECTION_STATE_CONNECTED == GetState())
    {
        RASPPPIP projectionInfo;
        memset(&projectionInfo, 0, sizeof(projectionInfo));
        projectionInfo.dwSize = sizeof(projectionInfo);
        DWORD projectionInfoSize = sizeof(projectionInfo);
        DWORD returnCode = RasGetProjectionInfo(m_rasConnection, RASP_PppIp, &projectionInfo, &projectionInfoSize);
        if (ERROR_SUCCESS != returnCode)
        {
            my_print(false, _T("RasGetProjectionInfo failed (%d)"), returnCode);
        }

        IPAddress = projectionInfo.szIpAddress;
    }

    return IPAddress;
}

bool VPNConnection::Establish(const tstring& serverAddress, const tstring& PSK)
{
    DWORD returnCode = ERROR_SUCCESS;

    Remove();

    if (m_state != VPN_CONNECTION_STATE_STOPPED && m_state != VPN_CONNECTION_STATE_FAILED)
    {
        my_print(false, _T("Invalid VPN connection state in Establish (%d)"), m_state);
        return false;
    }

    // The RasValidateEntryName function validates the format of a connection
    // entry name. The name must contain at least one non-white-space alphanumeric character.
    returnCode = RasValidateEntryName(0, VPN_CONNECTION_NAME);
    if (ERROR_SUCCESS != returnCode &&
        ERROR_ALREADY_EXISTS != returnCode)
    {
        my_print(false, _T("RasValidateEntryName failed (%d)"), returnCode);
        return false;
    }

    // Set up the VPN connection properties
    RASENTRY rasEntry;
    memset(&rasEntry, 0, sizeof(rasEntry));
    rasEntry.dwSize = sizeof(rasEntry);
    rasEntry.dwfOptions = RASEO_IpHeaderCompression |
                          RASEO_RemoteDefaultGateway |
                          RASEO_SwCompression |
                          RASEO_RequireMsEncryptedPw |
                          RASEO_RequireDataEncryption |
                          RASEO_ModemLights;
    rasEntry.dwfOptions2 = RASEO2_UsePreSharedKey |
                           RASEO2_DontNegotiateMultilink |
                           RASEO2_SecureFileAndPrint | 
                           RASEO2_SecureClientForMSNet |
                           RASEO2_DisableNbtOverIP;
    rasEntry.dwVpnStrategy = VS_L2tpOnly;
    lstrcpy(rasEntry.szLocalPhoneNumber, serverAddress.c_str());
    rasEntry.dwfNetProtocols = RASNP_Ip;
    rasEntry.dwFramingProtocol = RASFP_Ppp;
    rasEntry.dwEncryptionType = ET_Require;
    lstrcpy(rasEntry.szDeviceType, RASDT_Vpn);
        
    // The RasSetEntryProperties function changes the connection information
    // for an entry in the phone book or creates a new phone-book entry.
    // If the entry name does not match an existing entry, RasSetEntryProperties
    // creates a new phone-book entry.
    returnCode = RasSetEntryProperties(0, VPN_CONNECTION_NAME, &rasEntry, sizeof(rasEntry), 0, 0);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("RasSetEntryProperties failed (%d)"), returnCode);
        return false;
    }

    // Set the Preshared Secret
    RASCREDENTIALS vpnCredentials;
    memset(&vpnCredentials, 0, sizeof(vpnCredentials));
    vpnCredentials.dwSize = sizeof(vpnCredentials);
    vpnCredentials.dwMask = RASCM_PreSharedKey;
    lstrcpy(vpnCredentials.szPassword, PSK.c_str());
    returnCode = RasSetCredentials(0, VPN_CONNECTION_NAME, &vpnCredentials, FALSE);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("RasSetCredentials failed (%d)"), returnCode);
        return false;
    }

    // Make the vpn connection
    RASDIALPARAMS vpnParams;
    memset(&vpnParams, 0, sizeof(vpnParams));
    vpnParams.dwSize = sizeof(vpnParams);
    lstrcpy(vpnParams.szEntryName, VPN_CONNECTION_NAME);
    lstrcpy(vpnParams.szUserName, _T("user")); // The server does not care about username
    lstrcpy(vpnParams.szPassword, _T("password")); // This can also be hardcoded because
                                                   // the server authentication (which we
                                                   // really care about) is in IPSec using PSK

    // Pass pointer to this object to callback for state change updates
    vpnParams.dwCallbackId = (ULONG_PTR)this;

    m_rasConnection = 0;
    SetState(VPN_CONNECTION_STATE_STARTING);
    returnCode = RasDial(0, 0, &vpnParams, 2, &RasDialCallback, &m_rasConnection);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("RasDial failed (%d)"), returnCode);
        SetState(VPN_CONNECTION_STATE_FAILED);
        return false;
    }

    return true;
}

bool VPNConnection::Remove(void)
{
    // Don't remove the connection if we're upgrading -- we expect the
    // client to restart again and don't want a race between the old and
    // new processes to potentially mess with the new process's session.
    if (m_suspendTeardownForUpgrade)
    {
        return true;
    }

    DWORD returnCode = ERROR_SUCCESS;

    // Disconnect either the stored HRASCONN, or by entry name.
    // Based on sample code in:
    // http://msdn.microsoft.com/en-us/library/aa377284%28v=vs.85%29.aspx

    HRASCONN rasConnection = GetActiveRasConnection();

    if (!rasConnection)
    {
        // If there is no active RAS connection, we don't want to do anything.
        // We especially don't want to delete the phone book entry for a connection
        // that is connecting (but not active so does not enumerate) because that will
        // result in an ERROR_PORT_NOT_AVAILABLE error from RasDial until a reboot.
        return true;
    }

    // Hang up
    returnCode = RasHangUp(rasConnection);
    if (ERROR_NO_CONNECTION == returnCode)
    {
        return true;
    }
    else if (ERROR_SUCCESS != returnCode)
    {
        my_print(false, _T("RasHangUp failed (%d)"), returnCode);

        // Don't delete entry when in this state -- Windows gets confused
        return false;
    }

    // NOTE: no explicit state change to STOPPED; we're assuming RasDialCallback will set it

    RASCONNSTATUS status;
    memset(&status, 0, sizeof(status));
    status.dwSize = sizeof(status);
    // Wait until the connection has been terminated.
    // See the remarks here:
    // http://msdn.microsoft.com/en-us/library/aa377567(VS.85).aspx
    const int sleepTime = 100; // milliseconds
    const int maxSleepTime = 5000; // 5 seconds max wait time
    int totalSleepTime = 0;
    while(ERROR_INVALID_HANDLE != RasGetConnectStatus(rasConnection, &status))
    {
        Sleep(sleepTime);
        totalSleepTime += sleepTime;
        // Don't hang forever
        if (totalSleepTime >= maxSleepTime)
        {
            my_print(false, _T("RasHangUp/RasGetConnectStatus timed out (%d)"), GetLastError());

            // Don't delete entry when in this state -- Windows gets confused
            return false;
        }
    }

    // Delete the entry
    returnCode = RasDeleteEntry(0, VPN_CONNECTION_NAME);
    if (ERROR_SUCCESS != returnCode &&
        ERROR_CANNOT_FIND_PHONEBOOK_ENTRY != returnCode)
    {
        my_print(false, _T("RasDeleteEntry failed (%d)"), returnCode);
        return false;
    }

    m_rasConnection = 0;

    return true;
}

HRASCONN VPNConnection::GetActiveRasConnection()
{
    if (m_rasConnection)
    {
        return m_rasConnection;
    }

    // In case the application starts while the VPN connection is already active, 
    // we need to find the rasConnection by name.

    HRASCONN rasConnection = 0;
    RASCONN conn;
    memset(&conn, 0, sizeof(conn));
    conn.dwSize = sizeof(conn);
    LPRASCONN rasConnections = &conn;
    DWORD bufferSize = sizeof(conn);
    DWORD connections = 0;

    DWORD returnCode = RasEnumConnections(rasConnections, &bufferSize, &connections);

    // On Windows XP, we can't call RasEnumConnections with rasConnections = 0 because it
    // fails with 632 ERROR_INVALID_SIZE.  So we set rasConnections to point to a single
    // RASCONN, and we'll always reallocate, even if there is only one RASCONN and the
    // first call succeeds.
    if (ERROR_SUCCESS == returnCode)
    {
        returnCode = ERROR_BUFFER_TOO_SMALL;
    }

    // NOTE: Race condition where a new connection is added between the buffersize check
    //       and the second call.
    if (ERROR_BUFFER_TOO_SMALL != returnCode && connections > 0)
    {
        my_print(false, _T("RasEnumConnections failed (%d)"), returnCode);
    }
    else if (ERROR_BUFFER_TOO_SMALL == returnCode && connections > 0)
    {
        // See "A fix to work with older versions of Windows"
        if (bufferSize < (connections * sizeof(RASCONN)))
        {
            bufferSize = connections * sizeof(RASCONN);
        }

        // Allocate the memory needed for the array of RAS structure(s).
        rasConnections = (LPRASCONN)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufferSize);
        if (!rasConnections)
        {
            my_print(false, _T("HeapAlloc failed"));
            return rasConnection;
        }
 
        // The first RASCONN structure in the array must contain the RASCONN structure size
        rasConnections[0].dwSize = sizeof(RASCONN);
		
        // Call RasEnumConnections to enumerate active connections
        returnCode = RasEnumConnections(rasConnections, &bufferSize, &connections);

        // If successful, find the one with VPN_CONNECTION_NAME.
        if (ERROR_SUCCESS != returnCode)
        {
            my_print(false, _T("RasEnumConnections failed (%d)"), returnCode);
        }
        else
        {
            for (DWORD i = 0; i < connections; i++)
            {
                if (!_tcscmp(rasConnections[i].szEntryName, VPN_CONNECTION_NAME))
                {
                    // Entry name is unique and we found it
                    rasConnection = rasConnections[i].hrasconn;
                    break;
                }
		    }
        }

        //Deallocate memory for the connection buffer
        HeapFree(GetProcessHeap(), 0, rasConnections);
        rasConnections = 0;
    }

    return rasConnection;
}
