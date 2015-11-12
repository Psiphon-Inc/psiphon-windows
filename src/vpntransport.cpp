/*
 * Copyright (c) 2015, Psiphon Inc.
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
#include "vpntransport.h"
#include "logging.h"
#include "sessioninfo.h"
#include "psiclient.h"
#include "ras.h"
#include "raserror.h"
#include "utilities.h"
#include "server_request.h"
#include "diagnostic_info.h"


#define VPN_CONNECTION_TIMEOUT_SECONDS  20
#define VPN_CONNECTION_NAME             _T("Psiphon3")


void TweakVPN();
void TweakDNS();


// Support the registration of this transport type
static ITransport* New()
{
    return new VPNTransport();
}

// static
void VPNTransport::GetFactory(
                    tstring& o_transportDisplayName,
                    tstring& o_transportProtocolName,
                    TransportFactoryFn& o_transportFactory,
                    AddServerEntriesFn& o_addServerEntriesFn)
{
    o_transportFactory = New;
    o_transportDisplayName = VPN_TRANSPORT_DISPLAY_NAME;
    o_transportProtocolName = VPN_TRANSPORT_PROTOCOL_NAME;
    o_addServerEntriesFn = ITransport::AddServerEntries;
}


VPNTransport::VPNTransport()
    : ITransport(GetTransportProtocolName().c_str()),
      m_state(CONNECTION_STATE_STOPPED),
      m_stateChangeEvent(INVALID_HANDLE_VALUE),
      m_rasConnection(0),
      m_lastErrorCode(0)
{
    m_stateChangeEvent = CreateEvent(NULL, FALSE, FALSE, 0);
}

VPNTransport::~VPNTransport()
{
    IWorkerThread::Stop();

    // We must be careful about cleaning up if we're upgrading -- we expect the
    // client to restart again and don't want a race between the old and
    // new processes to potentially mess with the new process's session.
    // If we're upgrading, then we never made a connection in the first place.
    if (!m_rasConnection)
    {
        (void)Cleanup();
    }
    CloseHandle(m_stateChangeEvent);
}

tstring VPNTransport::GetTransportProtocolName() const 
{ 
    return VPN_TRANSPORT_PROTOCOL_NAME;
}

tstring VPNTransport::GetTransportDisplayName() const 
{ 
    return VPN_TRANSPORT_DISPLAY_NAME;
}

tstring VPNTransport::GetTransportRequestName() const
{
    return GetTransportProtocolName();
}

tstring VPNTransport::GetSessionID(const SessionInfo& sessionInfo)
{
    if (m_pppIPAddress.empty())
    {
        m_pppIPAddress = GetPPPIPAddress();
    }
    return m_pppIPAddress;
}

bool VPNTransport::RequiresStatsSupport() const
{
    return true;
}

tstring VPNTransport::GetLastTransportError() const
{
    tstringstream s;
    s << GetLastErrorCode();
    return s.str();
}

bool VPNTransport::IsHandshakeRequired() const
{
    return true;
}

bool VPNTransport::IsWholeSystemTunneled() const
{
    return true;
}

bool VPNTransport::ServerHasCapabilities(const ServerEntry& entry) const
{
    // VPN requires a pre-tunnel handshake

    bool canHandshake = ServerRequest::ServerHasRequestCapabilities(entry);

    return canHandshake && entry.HasCapability(WStringToUTF8(GetTransportProtocolName()));
}

bool VPNTransport::Cleanup()
{
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
        my_print(NOT_SENSITIVE, false, _T("RasHangUp failed (%d)"), returnCode);

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
            my_print(NOT_SENSITIVE, false, _T("RasHangUp/RasGetConnectStatus timed out (%d)"), GetLastError());

            // Don't delete entry when in this state -- Windows gets confused
            return false;
        }
    }

    // Delete the entry
    returnCode = RasDeleteEntry(0, VPN_CONNECTION_NAME);
    if (ERROR_SUCCESS != returnCode &&
        ERROR_CANNOT_FIND_PHONEBOOK_ENTRY != returnCode)
    {
        my_print(NOT_SENSITIVE, false, _T("RasDeleteEntry failed (%d)"), returnCode);
        return false;
    }

    m_rasConnection = 0;

    return true;
}

void VPNTransport::TransportConnect()
{
    // The SystemProxySettings member is unused

    // VPN should never be used for a temporary connection
    assert(!m_tempConnectServerEntry);

    if (!m_serverListReorder.IsRunning())
    {
        m_serverListReorder.Start(&m_serverList);
    }

    if (m_firstConnectionAttempt)
    {
        my_print(NOT_SENSITIVE, false, _T("%s connecting to server..."), GetTransportDisplayName().c_str());
    }
    else
    {
        my_print(NOT_SENSITIVE, false, _T("%s connecting to next server of %d known servers..."), GetTransportDisplayName().c_str(), GetConnectionServerEntryCount());
    }

    try
    {
        TransportConnectHelper();
    }
    catch(...)
    {
        (void)Cleanup();

        if (!m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons, false))
        {
            my_print(NOT_SENSITIVE, false, _T("%s server connection failed."), GetTransportDisplayName().c_str());
        }

        throw;
    }

    my_print(NOT_SENSITIVE, false, _T("%s successfully connected."), GetTransportDisplayName().c_str());
}

void VPNTransport::TransportConnectHelper()
{
    //
    // Minimum version check for VPN
    // - L2TP/IPSec/PSK not supported on Windows 2000
    //
    
    OSVERSIONINFO versionInfo;
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    if (!GetVersionEx(&versionInfo) ||
            versionInfo.dwMajorVersion < 5 ||
            (versionInfo.dwMajorVersion == 5 && versionInfo.dwMinorVersion == 0))
    {
        my_print(NOT_SENSITIVE, false, _T("VPN requires Windows XP or greater"));

        // Cause this transport's connect sequence (and immediate failover) to 
        // stop. Otherwise we'll quickly fail over and over.
        m_stopInfo.stopSignal->SignalStop(STOP_REASON_CANCEL);
        throw Abort();
    }

    ServerEntry serverEntry;
    if (!GetConnectionServerEntry(serverEntry))
    {
        my_print(NOT_SENSITIVE, false, _T("No known servers support this transport type."));

        // Cause this transport's connect sequence (and immediate failover) to 
        // stop. Otherwise we'll quickly fail over and over.
        m_stopInfo.stopSignal->SignalStop(STOP_REASON_CANCEL);
        throw Abort();
    }

    SessionInfo sessionInfo;
    sessionInfo.Set(serverEntry);

    // Record which server we're attempting to connect to
    Json::Value json;
    json["ipAddress"] = sessionInfo.GetServerAddress();
    AddDiagnosticInfoJson("ConnectingServer", json);

    // Do pre-handshake

    if (!DoHandshake(
            true,  // pre-handshake
            sessionInfo))
    {
        MarkServerFailed(sessionInfo.GetServerEntry());
        throw TransportFailed();
    }

    //
    // Check VPN services and fix if required/possible
    //
    
    // Note: we proceed even if the call fails. Testing is inconsistent -- don't
    // always need all tweaks to connect.
    TweakVPN();

    //
    // Start VPN connection
    //

    if (!Establish(
            UTF8ToWString(sessionInfo.GetServerAddress()), 
            UTF8ToWString(sessionInfo.GetPSK())))
    {
        MarkServerFailed(sessionInfo.GetServerEntry());
        throw TransportFailed();
    }

    //
    // Monitor VPN connection and wait for CONNECTED or FAILED
    //
    
    // Also, wait no longer than VPN_CONNECTION_TIMEOUT_SECONDS... overriding any system
    // configuration built-in VPN client timeout (which we've found to be too long -- over a minute).
    if (!WaitForConnectionStateToChangeFrom(
            CONNECTION_STATE_STARTING, 
            VPN_CONNECTION_TIMEOUT_SECONDS*1000))
    {
        MarkServerFailed(sessionInfo.GetServerEntry());
        throw TransportFailed();
    }
    
    if (CONNECTION_STATE_CONNECTED != GetConnectionState())
    {
        // Note: WaitForConnectionStateToChangeFrom throws Abort if user
        // cancelled, so if we're here it's a FAILED case.
        MarkServerFailed(sessionInfo.GetServerEntry());
        throw TransportFailed();
    }

    // The connection is good.
    MarkServerSucceeded(sessionInfo.GetServerEntry());
    m_sessionInfo = sessionInfo;

    //
    // Patch DNS bug on Windowx XP; and flush DNS
    // to ensure domains are resolved with VPN's DNS server
    //
    
    // Note: we proceed even if the call fails. This means some domains
    // may not resolve properly.
    TweakDNS();
}


bool VPNTransport::GetConnectionServerEntry(ServerEntry& o_serverEntry)
{
    // Return the first ServerEntry that can be used. This will encourage
    // server affinity (i.e., using the last successful server).

    ServerEntries serverEntries = m_serverList.GetList();

    for (ServerEntryIterator it = serverEntries.begin();
         it != serverEntries.end();
         ++it)
    {
        if (ServerHasCapabilities(*it))
        {
            o_serverEntry = *it;
            return true;
        }
    }

    return false;
}


size_t VPNTransport::GetConnectionServerEntryCount()
{
    // Return the first ServerEntry that can be used. This will encourage
    // server affinity (i.e., using the last successful server).

    ServerEntries serverEntries = m_serverList.GetList();
    size_t count = 0;

    for (ServerEntryIterator it = serverEntries.begin();
         it != serverEntries.end();
         ++it)
    {
        if (ServerHasCapabilities(*it))
        {
            count++;
        }
    }

    return count;
}


bool VPNTransport::Establish(const tstring& serverAddress, const tstring& PSK)
{
    DWORD returnCode = ERROR_SUCCESS;

    (void)Cleanup();

    if (GetConnectionState() != CONNECTION_STATE_STOPPED && GetConnectionState() != CONNECTION_STATE_FAILED)
    {
        my_print(NOT_SENSITIVE, false, _T("Invalid VPN connection state in Establish (%d)"), GetConnectionState());
        return false;
    }

    // The RasValidateEntryName function validates the format of a connection
    // entry name. The name must contain at least one non-white-space alphanumeric character.
    returnCode = RasValidateEntryName(0, VPN_CONNECTION_NAME);
    if (ERROR_SUCCESS != returnCode &&
        ERROR_ALREADY_EXISTS != returnCode)
    {
        my_print(NOT_SENSITIVE, false, _T("RasValidateEntryName failed (%d)"), returnCode);
        SetLastErrorCode(returnCode);
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
        my_print(NOT_SENSITIVE, false, _T("RasSetEntryProperties failed (%d)"), returnCode);
        SetLastErrorCode(returnCode);
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
        my_print(NOT_SENSITIVE, false, _T("RasSetCredentials failed (%d)"), returnCode);
        SetLastErrorCode(returnCode);
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
    SetConnectionState(CONNECTION_STATE_STARTING);
    returnCode = RasDial(0, 0, &vpnParams, 2, &(VPNTransport::RasDialCallback), &m_rasConnection);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(NOT_SENSITIVE, false, _T("RasDial failed (%d)"), returnCode);
        SetConnectionState(CONNECTION_STATE_FAILED);
        SetLastErrorCode(returnCode);
        return false;
    }

    return true;
}

VPNTransport::ConnectionState VPNTransport::GetConnectionState() const
{
    return m_state;
}

void VPNTransport::SetConnectionState(ConnectionState newState)
{
    m_state = newState;
    SetEvent(m_stateChangeEvent);
}

HANDLE VPNTransport::GetStateChangeEvent()
{
    return m_stateChangeEvent;
}

void VPNTransport::SetLastErrorCode(unsigned int lastErrorCode) 
{
    m_lastErrorCode = lastErrorCode;
}

unsigned int VPNTransport::GetLastErrorCode(void) const
{
    return m_lastErrorCode;
}

bool VPNTransport::DoPeriodicCheck()
{
    return GetConnectionState() == CONNECTION_STATE_CONNECTED;
}

bool VPNTransport::WaitForConnectionStateToChangeFrom(ConnectionState state, DWORD timeout)
{
    DWORD totalWaitMilliseconds = 0;

    ConnectionState originalState = state;

    while (state == GetConnectionState())
    {
        // Wait for RasDialCallback to set a new state, or timeout (to check cancel/termination)

        DWORD waitMilliseconds = 100;

        DWORD result = WaitForSingleObject(GetStateChangeEvent(), waitMilliseconds);

        totalWaitMilliseconds += waitMilliseconds;

        if (result == WAIT_TIMEOUT)
        {
            if (totalWaitMilliseconds >= timeout)
                return false;
        }
        else if (result == WAIT_OBJECT_0)
        {
            // State event set, but that doesn't mean that the state actually changed.
            // Let the loop condition check.
            continue;
        }
        else if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons, false)) // stop event signalled
        {
            // TODO: Maybe this should let CheckSignalStop throw
            throw Abort();
        }
        else
        {
            std::stringstream s;
            s << __FUNCTION__ << ": WaitForMultipleObjects failed (" << result << ", " << GetLastError() << ")";
            throw Error(s.str().c_str());
        }
    }

    return true;
}

tstring VPNTransport::GetPPPIPAddress() const
{
    tstring IPAddress;

    if (m_rasConnection && CONNECTION_STATE_CONNECTED == GetConnectionState())
    {
        RASPPPIP projectionInfo;
        memset(&projectionInfo, 0, sizeof(projectionInfo));
        projectionInfo.dwSize = sizeof(projectionInfo);
        DWORD projectionInfoSize = sizeof(projectionInfo);
        DWORD returnCode = RasGetProjectionInfo(m_rasConnection, RASP_PppIp, &projectionInfo, &projectionInfoSize);
        if (ERROR_SUCCESS != returnCode)
        {
            my_print(NOT_SENSITIVE, false, _T("RasGetProjectionInfo failed (%d)"), returnCode);
        }

        IPAddress = projectionInfo.szIpAddress;
    }

    return IPAddress;
}

HRASCONN VPNTransport::GetActiveRasConnection()
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
        my_print(NOT_SENSITIVE, false, _T("RasEnumConnections failed (%d)"), returnCode);
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
            my_print(NOT_SENSITIVE, false, _T("HeapAlloc failed"));
            return rasConnection;
        }
 
        // The first RASCONN structure in the array must contain the RASCONN structure size
        rasConnections[0].dwSize = sizeof(RASCONN);
        
        // Call RasEnumConnections to enumerate active connections
        returnCode = RasEnumConnections(rasConnections, &bufferSize, &connections);

        // If successful, find the one with VPN_CONNECTION_NAME.
        if (ERROR_SUCCESS != returnCode)
        {
            my_print(NOT_SENSITIVE, false, _T("RasEnumConnections failed (%d)"), returnCode);
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

void CALLBACK VPNTransport::RasDialCallback(
    DWORD userData,
    DWORD,
    HRASCONN rasConnection,
    UINT,
    RASCONNSTATE rasConnState,
    DWORD dwError,
    DWORD)
{
    VPNTransport* vpnTransport = (VPNTransport*)userData;

    my_print(NOT_SENSITIVE, true, _T("RasDialCallback (%x %d)"), rasConnState, dwError);
    
    if (0 != dwError)
    {
        const DWORD errorStringSize = 1024;
        TCHAR errorString[errorStringSize];
        if (RasGetErrorString(dwError, errorString, errorStringSize) != ERROR_SUCCESS)
        {
            errorString[0] = _T('\0');
        }

        my_print(NOT_SENSITIVE, false, _T("VPN connection failed: %s (%d)"), errorString, dwError);
        vpnTransport->SetConnectionState(CONNECTION_STATE_FAILED);
        vpnTransport->SetLastErrorCode(dwError);
    }
    else if (RASCS_Connected == rasConnState)
    {
        // Set up a disconnection notification event
        AutoHANDLE rasEvent = CreateEvent(0, FALSE, FALSE, 0);
        DWORD returnCode = RasConnectionNotification(rasConnection, rasEvent, RASCN_Disconnection);
        if (ERROR_SUCCESS != returnCode)
        {
            my_print(NOT_SENSITIVE, false, _T("RasConnectionNotification failed (%d)"), returnCode);
            return;
        }

        vpnTransport->SetConnectionState(CONNECTION_STATE_CONNECTED);

        if (WAIT_FAILED == WaitForSingleObject(rasEvent, INFINITE))
        {
            my_print(NOT_SENSITIVE, false, _T("WaitForSingleObject failed (%d)"), GetLastError());
            // Fall through to VPN_CONNECTION_STATE_STOPPED.
            // Otherwise we'd be stuck in a connected state.
        }

        vpnTransport->SetConnectionState(CONNECTION_STATE_STOPPED);
    }
    else
    {
        my_print(NOT_SENSITIVE, true, _T("VPN establishing connection... (%x)"), rasConnState);
        vpnTransport->SetConnectionState(CONNECTION_STATE_STARTING);
    }
}


//==== TweakVPN utility functions =============================================

void FixProhibitIpsec()
{
    // Check for non-default HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\RasMan\Parameters\ProhibitIpSec = 1
    // If found, try to set to 0
    // In testing, we've found the setting of 1 *sometimes* results in a failed connection, so
    // we try to change it to 0 but we don't abort the VPN connection attempt if it's 1 and we can't change it.

    std::stringstream error;
    HKEY key = 0;

    try
    {
        const char* keyName = "SYSTEM\\CurrentControlSet\\Services\\RasMan\\Parameters";
        const char* valueName = "ProhibitIpSec";

        LONG returnCode = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyName, 0, KEY_READ, &key);

        if (ERROR_SUCCESS != returnCode)
        {
            error << "Open Registry Key failed (" << returnCode << ")";
            throw std::exception(error.str().c_str());
        }

        DWORD value;
        DWORD bufferLength = sizeof(value);
        DWORD type;
        
        returnCode = RegQueryValueExA(key, valueName, 0, &type, (LPBYTE)&value, &bufferLength);

        if (ERROR_SUCCESS != returnCode || type != REG_DWORD)
        {
            if (returnCode == ERROR_FILE_NOT_FOUND)
            {
                // The prohibitIpsec value isn't present by default, handle the same
                // as when value is present and set to 0
                value = 0;
            }
            else
            {
                error << "Query Registry Value failed (" << returnCode << ")";
                throw std::exception(error.str().c_str());
            }
        }

        if (value != 0)
        {
            value = 0;

            // Re-open the registry key with write privileges

            RegCloseKey(key);
            returnCode = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyName, 0, KEY_WRITE, &key);

            if (ERROR_SUCCESS != returnCode)
            {
                // If the user isn't Admin, this will fail as HKLM isn't writable by limited users
                if (ERROR_ACCESS_DENIED == returnCode)
                {
                    error << "insufficient privileges to set HKLM\\SYSTEM\\CurrentControlSet\\Services\\RasMan\\Parameters\\ProhibitIpSec to 0";
                }
                else
                {
                    error << "Open Registry Key failed (" << returnCode << ")";
                }

                throw std::exception(error.str().c_str());
            }

            returnCode = RegSetValueExA(key, valueName, 0, REG_DWORD, (PBYTE)&value, bufferLength);
            if (ERROR_SUCCESS != returnCode)
            {
                // TODO: add descriptive case for ACCESS_DENIED as above(?)
                error << "Set Registry Value failed (" << returnCode << ")";
                throw std::exception(error.str().c_str());
            }
        }
    }
    catch(std::exception& ex)
    {
        my_print(NOT_SENSITIVE, false, string("Fix ProhibitIpSec failed: ") + ex.what());
    }

    // cleanup
    RegCloseKey(key);
}

void FixVPNServices()
{
    // Check for disabled IPSec services and attempt to restore to
    // default (enabled, auto-start) config and start

    // The start configuration for the "Policy Agent" service
    // is different for Windows 7.

    DWORD policyAgentStartType = SERVICE_AUTO_START;
    OSVERSIONINFO versionInfo;
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    // Version 6.1 is Windows 7
    if (GetVersionEx(&versionInfo) &&
        versionInfo.dwMajorVersion > 6 ||
        (versionInfo.dwMajorVersion == 6 && versionInfo.dwMinorVersion >= 1))
    {
        policyAgentStartType = SERVICE_DEMAND_START;
    }

    struct serviceConfig
    {
        const TCHAR* name;
        DWORD startType;
    } serviceConfigs[] =
    {
        {_T("IKEEXT"), SERVICE_AUTO_START},
        {_T("PolicyAgent"), policyAgentStartType},
        {_T("RasMan"), SERVICE_DEMAND_START},
        {_T("TapiSrv"), SERVICE_DEMAND_START}
    };

    for (int i = 0; i < sizeof(serviceConfigs)/sizeof(serviceConfig); i++)
    {
        std::stringstream error;
        SC_HANDLE manager = NULL;
        SC_HANDLE service = NULL;

        try
        {
            if (manager != NULL)
            {
                CloseServiceHandle(manager);
                manager = NULL;
            }

            manager = OpenSCManager(NULL, NULL, GENERIC_READ);
            if (NULL == manager)
            {
                error << "OpenSCManager failed (" << GetLastError() << ")";
                throw std::exception(error.str().c_str());
            }

            if (service != NULL)
            {
                CloseServiceHandle(service);
                service = NULL;
            }

            service = OpenService(manager,
                                  serviceConfigs[i].name,
                                  SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS);
            if (NULL == service)
            {
                if (ERROR_SERVICE_DOES_NOT_EXIST == GetLastError())
                {
                    // Service doesn't exist, which is expected; e.g., Windows XP doesn't have IKEEXT
                    // In the case where the service *should* exist, there's nothing we can do
                    continue;
                }

                error << "OpenService failed (" << GetLastError() << ")";
                throw std::exception(error.str().c_str());
            }

            BYTE buffer[8192]; // Max size is 8K: http://msdn.microsoft.com/en-us/library/ms684932%28v=vs.85%29.aspx
            DWORD bufferLength = sizeof(buffer);
            QUERY_SERVICE_CONFIG* serviceConfig = (QUERY_SERVICE_CONFIG*)buffer;
            if (!QueryServiceConfig(service, serviceConfig, bufferLength, &bufferLength))
            {
                error << "QueryServiceConfig failed (" << GetLastError() << ")";
                throw std::exception(error.str().c_str());
            }

            SERVICE_STATUS serviceStatus;
            if (!QueryServiceStatus(service, &serviceStatus))
            {
                error << "QueryServiceStatus failed (" << GetLastError() << ")";
                throw std::exception(error.str().c_str());
            }

            // Note: won't downgrade from SERVICE_AUTO_START to SERVICE_DEMAND_START
            bool needConfigChange = (SERVICE_AUTO_START != serviceConfig->dwStartType &&
                                     serviceConfigs[i].startType != serviceConfig->dwStartType);

            // Automatic services are started at login, but we're not re-logging in, so start if
            // it should've been started at login.
            // Note: only start if "required" start type is auto, not current config start type
            bool needStart = (serviceConfigs[i].startType == SERVICE_AUTO_START &&
                              serviceStatus.dwCurrentState != SERVICE_RUNNING);

            if (needConfigChange || needStart)
            {
                CloseServiceHandle(manager);
                manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
                if (NULL == manager)
                {
                    // If the user isn't Admin, this will fail
                    if (ERROR_ACCESS_DENIED == GetLastError())
                    {
                        error << "insufficient privileges to configure or start service " <<
                                    WStringToUTF8(serviceConfigs[i].name);
                    }
                    else
                    {
                        error << "OpenSCManager failed (" << GetLastError() << ")";
                    }
                    throw std::exception(error.str().c_str());
                }

                CloseServiceHandle(service);
                service = OpenService(manager,
                                      serviceConfigs[i].name,
                                      SERVICE_CHANGE_CONFIG|SERVICE_START|SERVICE_QUERY_STATUS);
                if (NULL == service)
                {
                    // TODO: add descriptive case for ACCESS_DENIED as above(?)
                    error << "OpenService failed (" << GetLastError() << ")";
                    throw std::exception(error.str().c_str());
                }

                if (needConfigChange)
                {
                    if (!ChangeServiceConfig(
                            service,
                            SERVICE_NO_CHANGE, serviceConfigs[i].startType, SERVICE_NO_CHANGE,
                            NULL, NULL, NULL, NULL, NULL, NULL, NULL))
                    {
                        error << "ChangeServiceConfig failed (" << GetLastError() << ")";
                        throw std::exception(error.str().c_str());
                    }
                }

                if (needStart)
                {
                    if (!StartService(service, 0, 0))
                    {
                        error << "StartService failed (" << GetLastError() << ")";
                        throw std::exception(error.str().c_str());
                    }

                    // Wait up to 2 seconds for service to start before proceeding with
                    // connect. If it fails to start, we just proceed anyway.

                    for (int wait = 0; wait < 20; wait++)
                    {
                        SERVICE_STATUS serviceStatus;
                        if (!QueryServiceStatus(service, &serviceStatus))
                        {
                            error << "QueryServiceStatus failed (" << GetLastError() << ")";
                            throw std::exception(error.str().c_str());
                        }

                        // StartService changes the state to SERVICE_START_PENDING
                        // (http://msdn.microsoft.com/en-us/library/ms686321%28v=vs.85%29.aspx)
                        // So as soon as we see a new state, we can proceed.

                        if (serviceStatus.dwCurrentState != SERVICE_START_PENDING)
                        {
                            break;
                        }

                        Sleep(100);
                    }
                }
            }
        }
        catch(std::exception& ex)
        {
            my_print(NOT_SENSITIVE, false, string("Fix VPN Services failed: ") + ex.what());
        }

        // cleanup
        CloseServiceHandle(service); 
        CloseServiceHandle(manager);
    }
}

void TweakVPN()
{
    // Some 3rd party VPN clients change the default Windows system configuration in ways that
    // will prevent standard IPSec VPN, such as ours, from running.  We check for the issues
    // and try to fix if required/possible (needs admin privs).

    // Proceed regardless of FixProhibitIpSec/FixVPNServices success, as we're not sure
    // the fixes are always required.

    FixProhibitIpsec();
    FixVPNServices();
}

//==== TweakDNS utility functions =============================================

// memmem.c from gnulib. Used for short buffers -- quadratic performance not an issue.

/* Copyright (C) 1991,92,93,94,96,97,98,2000,2004,2007 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Return the first occurrence of NEEDLE in HAYSTACK.  */
static const void* memmem(
    const void* haystack,
    size_t haystack_len,
    const void* needle,
    size_t needle_len)
{
    const char *begin;
    const char *const last_possible = (const char *) haystack + haystack_len - needle_len;

    if (needle_len == 0)
    {
        /* The first occurrence of the empty string is deemed to occur at
           the beginning of the string.  */
        return (void *) haystack;
    }

    /* Sanity check, otherwise the loop might search through the whole
       memory.  */
    if (haystack_len < needle_len)
    {
        return NULL;
    }

    for (begin = (const char *) haystack; begin <= last_possible; ++begin)
    {
        if (begin[0] == ((const char *) needle)[0] &&
            !memcmp((const void *) &begin[1],
                    (const void *) ((const char *) needle + 1),
                    needle_len - 1))
        {
            return (void *) begin;
        }
    }

    return NULL;
}

static void PatchDNS()
{
    // Programmatically apply Window XP fix that ensures
    // VPN's DNS server is used (http://support.microsoft.com/kb/311218)

    OSVERSIONINFO versionInfo;
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);

    // Version 5.1 is Windows XP (not 64-bit)
    if (GetVersionEx(&versionInfo) &&
        versionInfo.dwMajorVersion == 5 &&
        versionInfo.dwMinorVersion == 1)
    {
        std::stringstream error;
        HKEY key = NULL;
        char *buffer = NULL;

        try
        {
            const char* keyName = "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Linkage";
            const char* valueName = "Bind";

            LONG returnCode = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyName, 0, KEY_READ, &key);

            if (ERROR_SUCCESS != returnCode)
            {
                error << "Open Registry Key failed (" << returnCode << ")";
                throw std::exception(error.str().c_str());
            }

            // RegQueryValueExA on Windows XP wants at least 1 byte
            buffer = new char [1];
            if (buffer == NULL)
            {
                error << __FUNCTION__ << ":" << __LINE__ << ": Out of memory";
                throw std::exception(error.str().c_str());
            }

            DWORD bufferLength = 1;
            DWORD type;
            
            // Using the ANSI version explicitly so we can manipulate narrow strings.
            returnCode = RegQueryValueExA(key, valueName, 0, &type, (LPBYTE)buffer, &bufferLength);
            if (ERROR_MORE_DATA == returnCode)
            {
                delete [] buffer;

                buffer = new char [bufferLength];
                if (buffer == NULL)
                {
                    error << __FUNCTION__ << ":" << __LINE__ << ": Out of memory";
                    throw std::exception(error.str().c_str());
                }

                returnCode = RegQueryValueExA(key, valueName, 0, 0, (LPBYTE)buffer, &bufferLength);
            }

            if (ERROR_SUCCESS != returnCode || type != REG_MULTI_SZ)
            {
                error << "Query Registry Value failed (" << returnCode << ")";
                throw std::exception(error.str().c_str());
            }

            // We must ensure that the string is double null terminated, as per MSDN
            // 2 bytes for 2 NULLs as it's a MULTI_SZ.
            int extraNulls = 0;
            if (buffer[bufferLength-1] != '\0')
            {
                extraNulls = 2;
            }
            else if (buffer[bufferLength-2] != '\0')
            {
                extraNulls = 1;
            }
            if (extraNulls)
            {
                char *newBuffer = new char [bufferLength + extraNulls];
                if (newBuffer == NULL)
                {
                    error << __FUNCTION__ << ":" << __LINE__ << ": Out of memory";
                    throw std::exception(error.str().c_str());
                }

                memset(newBuffer, bufferLength + extraNulls, 0);
                memcpy(newBuffer, buffer, bufferLength);
                bufferLength += extraNulls;
                delete [] buffer;
                buffer = newBuffer;
            }

            // Find the '\Device\NdisWanIp' string and move it to the first position.
            // (If it's already first, don't modify the registry).

            const char* target = "\\Device\\NdisWanIp";
            size_t target_length = strlen(target) + 1; // include '\0' terminator
            const char* found = (const char*)memmem(buffer, bufferLength, target, target_length);

            if (found && found != buffer)
            {
                // make new buffer = target || start of buffer to target || buffer after target
                char *newBuffer = new char [bufferLength];
                if (newBuffer == NULL)
                {
                    error << __FUNCTION__ << ":" << __LINE__ << ": Out of memory";
                    throw std::exception(error.str().c_str());
                }

                memcpy(newBuffer, found, target_length);
                memcpy(newBuffer + target_length, buffer, found - buffer);
                memcpy(newBuffer + target_length + (found - buffer),
                       found + target_length,
                       bufferLength - (target_length + (found - buffer)));
                delete [] buffer;
                buffer = newBuffer;

                // Re-open the registry key with write privileges

                RegCloseKey(key);
                returnCode = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyName, 0, KEY_WRITE, &key);

                if (ERROR_SUCCESS != returnCode)
                {
                    // If the user isn't Admin, this will fail as HKLM isn't writable by limited users
                    if (ERROR_ACCESS_DENIED == returnCode)
                    {
                        error << "insufficient privileges (See KB311218)";
                    }
                    else
                    {
                        error << "Open Registry Key failed (" << returnCode << ")";
                    }
                    throw std::exception(error.str().c_str());
                }

                returnCode = RegSetValueExA(key, valueName, 0, REG_MULTI_SZ, (PBYTE)buffer, bufferLength);
                if (ERROR_SUCCESS != returnCode)
                {
                    // TODO: add descriptive case for ACCESS_DENIED as above(?)
                    error << "Set Registry Value failed (" << returnCode << ")";
                    throw std::exception(error.str().c_str());
                }
            }
        }
        catch(std::exception& ex)
        {
            my_print(NOT_SENSITIVE, false, string("Fix DNS failed: ") + ex.what());
        }

        // cleanup
        if (buffer != NULL)
        {
            delete [] buffer;
        }

        RegCloseKey(key);
    }
}

typedef BOOL (CALLBACK* DNSFLUSHPROC)();

static bool FlushDNS()
{
    // Adapted code from: http://www.codeproject.com/KB/cpp/Setting_DNS.aspx

    bool result = false;
    HINSTANCE hDnsDll;
    DNSFLUSHPROC pDnsFlushProc;

    if ((hDnsDll = LoadLibrary(_T("dnsapi"))) == NULL)
    {
        my_print(NOT_SENSITIVE, false, _T("LoadLibrary DNSAPI failed"));
        return result;
    }

    if ((pDnsFlushProc = (DNSFLUSHPROC)GetProcAddress(hDnsDll, "DnsFlushResolverCache")) != NULL)
    {
        if (FALSE == (pDnsFlushProc)())
        {
            my_print(NOT_SENSITIVE, false, _T("DnsFlushResolverCache failed: %d"), GetLastError());
        }
        else
        {
            result = true;
        }
    }
    else
    {
        my_print(NOT_SENSITIVE, false, _T("GetProcAddress DnsFlushResolverCache failed"));
    }

    FreeLibrary(hDnsDll);
    return result;
}

void TweakDNS()
{
    // Note: no lock

    // Patch tries to fix a bug on XP where the non-VPN's DNS server is still consulted
    PatchDNS();

    // Flush is to clear cached lookups from non-VPN DNS
    // Note: this only affects system cache, not application caches (e.g., browsers)
    FlushDNS();
}
