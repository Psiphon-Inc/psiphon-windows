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
#include "coretransport.h"
#include "sessioninfo.h"
#include "psiclient.h"
#include <WinSock2.h>
#include <WinCrypt.h>
#include "utilities.h"
#include "systemproxysettings.h"
#include "config.h"
#include "diagnostic_info.h"
#include <stdlib.h>
#include <time.h>


#define AUTOMATICALLY_ASSIGNED_PORT_NUMBER   0
#define EXE_NAME                             _T("psiphon-tunnel-core.exe")

static const TCHAR* SSH_TRANSPORT_PROTOCOL_NAME = _T("SSH");
static const TCHAR* SSH_TRANSPORT_DISPLAY_NAME = _T("SSH");

static const TCHAR* OSSH_TRANSPORT_PROTOCOL_NAME = _T("OSSH");
static const TCHAR* OSSH_TRANSPORT_DISPLAY_NAME = _T("SSH+");

/******************************************************************************
 CoreTransportBase
******************************************************************************/

CoreTransportBase::CoreTransportBase(LPCTSTR transportProtocolName)
    : ITransport(transportProtocolName),
      m_pipe(NULL),
      m_localSocksProxyPort(AUTOMATICALLY_ASSIGNED_PORT_NUMBER),
      m_localHttpProxyPort(AUTOMATICALLY_ASSIGNED_PORT_NUMBER)
{
    ZeroMemory(&m_processInfo, sizeof(m_processInfo));
}


CoreTransportBase::~CoreTransportBase()
{
    (void)Cleanup();
}


bool CoreTransportBase::Cleanup()
{
     // Give the process an opportunity for graceful shutdown, then terminate
    if (m_processInfo.hProcess != 0
        && m_processInfo.hProcess != INVALID_HANDLE_VALUE)
    {
        StopProcess(m_processInfo.dwProcessId, m_processInfo.hProcess);
    }

    if (m_processInfo.hProcess != 0
        && m_processInfo.hProcess != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_processInfo.hProcess);
    }
    ZeroMemory(&m_processInfo, sizeof(m_processInfo));

    if (m_pipe!= 0
        && m_pipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_pipe);
    }
    m_pipe = NULL;

    return true;
}


bool CoreTransportBase::IsHandshakeRequired() const
{
    return false;
}


bool CoreTransportBase::IsWholeSystemTunneled() const
{
    return false;
}


bool CoreTransportBase::IsSplitTunnelSupported() const
{
    // TODO: support may be added to core
    return false;
}


bool CoreTransportBase::ServerWithCapabilitiesExists()
{
    // For now, we assume there are sufficient server entries for SSH/OSSH.
    // Even if there are not any in the core database, it will automatically
    // perform its own remote server list fetch.
    return true;
}


bool CoreTransportBase::ServerHasCapabilities(const ServerEntry& entry) const
{
    // Should not be called for core.
    assert(false);
    return false;
}


bool CoreTransportBase::RequiresStatsSupport() const
{
    return false;
}


tstring CoreTransportBase::GetSessionID(const SessionInfo& sessionInfo)
{
    return NarrowToTString(sessionInfo.GetSSHSessionID());
}


int CoreTransportBase::GetLocalProxyParentPort() const
{
    // Should not be called for core.
    assert(false);
    return 0;
}


tstring CoreTransportBase::GetLastTransportError() const
{
    return _T("0");
}


bool CoreTransportBase::DoPeriodicCheck()
{
    // Check if the subprocess is still running, and consume any buffered output

    if (m_processInfo.hProcess != 0)
    {
        // The process handle will be signalled when the process terminates
        DWORD result = WaitForSingleObject(m_processInfo.hProcess, 0);

        if (result == WAIT_TIMEOUT)
        {
            if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
            {
                throw Abort();
            }

            ConsumeSubprocessOutput();

            return true;
        }
        else if (result == WAIT_OBJECT_0)
        {
            // The process has signalled -- which implies that it's died
            return false;
        }
        else
        {
            std::stringstream s;
            s << __FUNCTION__ << ": WaitForSingleObject failed (" << result << ", " << GetLastError() << ")";
            throw Error(s.str().c_str());
        }
    }

    return false;
}


void CoreTransportBase::ConsumeSubprocessOutput()
{
    //***************************************************************************
    // TODO: parse output and get ports, home pages, #tunnels, etc.
    //***************************************************************************

    //***************************************************************************
    // TODO: log raw output; plus structured data: AddDiagnosticInfoYaml("ConnectedServer")
    //***************************************************************************

    /*
    ostringstream ss;
    ss << "ipAddress: " << m_sessionInfo.GetServerAddress() << "\n";
    ss << "connType: " << TStringToNarrow(transportRequestName) << "\n";
    if (serverAddress != NarrowToTString(m_sessionInfo.GetServerAddress())) 
    {
        ss << "front: " << TStringToNarrow(serverAddress) << "\n";
    }
    AddDiagnosticInfoYaml("ConnectedServer", ss.str().c_str());
    */

    // TODO: AddDiagnosticInfoYaml
}


void CoreTransportBase::TransportConnect()
{
    my_print(NOT_SENSITIVE, false, _T("%s connecting..."), GetTransportDisplayName().c_str());

    try
    {
        TransportConnectHelper();
    }
    catch(...)
    {
        (void)Cleanup();

        if (!m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons, false))
        {
            my_print(NOT_SENSITIVE, false, _T("%s connection failed."), GetTransportDisplayName().c_str());
        }

        throw;
    }

    my_print(NOT_SENSITIVE, false, _T("%s successfully connected."), GetTransportDisplayName().c_str());
}


void CoreTransportBase::TransportConnectHelper()
{
    assert(m_systemProxySettings != NULL);

    //***************************************************************************
    // TODO: pave app data subdir; pave config file (here is where to call GetUpstreamProxySettings)
    //***************************************************************************

    //***************************************************************************
    // TODO: use m_tempConnectServerEntry when set
    //***************************************************************************

    if (!SpawnSubprocess())
    {
        throw TransportFailed();
    }

    //***************************************************************************
    // TODO: monitor output and wait for TUNNELS > 0; or throw when stop signalled
    //***************************************************************************
    //WaitForActiveTunnel();

    m_systemProxySettings->SetSocksProxyPort(m_localSocksProxyPort);
    my_print(NOT_SENSITIVE, false, _T("SOCKS proxy is running on localhost port %d."), m_localSocksProxyPort);

    m_systemProxySettings->SetHttpProxyPort(m_localHttpProxyPort);
    my_print(NOT_SENSITIVE, false, _T("HTTP proxy is running on localhost port %d."), m_localHttpProxyPort);
}


bool CoreTransportBase::SpawnSubprocess()
{
    tstringstream commandLine;

    if (m_exePath.size() == 0)
    {
        if (!ExtractExecutable(IDR_PSIPHON_TUNNEL_CORE_EXE, EXE_NAME, m_exePath))
        {
            return false;
        }
    }

    commandLine << m_exePath;

    STARTUPINFO startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!CreateSubprocessPipe(startupInfo.hStdOutput, startupInfo.hStdError))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - CreateSubprocessPipe failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    m_pipe = INVALID_HANDLE_VALUE;
    startupInfo.hStdOutput = INVALID_HANDLE_VALUE;
    startupInfo.hStdError = INVALID_HANDLE_VALUE;

    HANDLE parentInputPipe = INVALID_HANDLE_VALUE;
    HANDLE childStdinPipe = INVALID_HANDLE_VALUE;

    if (!CreateSubprocessPipes(
            m_pipe,
            parentInputPipe,
            childStdinPipe,
            startupInfo.hStdOutput,
            startupInfo.hStdError))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - CreateSubprocessPipe failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    CloseHandle(parentInputPipe);
    CloseHandle(childStdinPipe);

    if (!CreateProcess(
            m_exePath.c_str(),
            (TCHAR*)commandLine.str().c_str(),
            NULL,
            NULL,
            TRUE, // bInheritHandles
#ifdef _DEBUG
            CREATE_NEW_PROCESS_GROUP,
#else
            CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW,
#endif
            NULL,
            NULL,
            &startupInfo,
            &m_processInfo))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - TunnelCore CreateProcess failed (%d)"), __TFUNCTION__, GetLastError());
        CloseHandle(m_pipe);
        CloseHandle(startupInfo.hStdOutput);
        CloseHandle(startupInfo.hStdError);
        return false;
    }

    // Close the unneccesary handles
    CloseHandle(m_processInfo.hThread);
    m_processInfo.hThread = NULL;

    // Close child pipe handle (do not continue to modify the parent).
    // You need to make sure that no handles to the write end of the
    // output pipe are maintained in this process or else the pipe will
    // not close when the child process exits and the ReadFile will hang.
    
    if (!CloseHandle(startupInfo.hStdOutput))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        CloseHandle(m_pipe);
        CloseHandle(startupInfo.hStdError);
        return false;
    }
    if (!CloseHandle(startupInfo.hStdError))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        CloseHandle(m_pipe);
        return false;
    }
    
    WaitForInputIdle(m_processInfo.hProcess, 5000);

    return true;
}


bool CoreTransportBase::CreateSubprocessPipe(HANDLE& o_outputPipe, HANDLE& o_errorPipe)
{
    m_pipe = INVALID_HANDLE_VALUE;
    o_outputPipe = INVALID_HANDLE_VALUE;
    o_errorPipe = INVALID_HANDLE_VALUE;

    HANDLE parentInputPipe = INVALID_HANDLE_VALUE, childStdinPipe = INVALID_HANDLE_VALUE;

    if (!CreateSubprocessPipes(
            m_pipe,
            parentInputPipe,
            childStdinPipe,
            o_outputPipe,
            o_errorPipe))
    {
        return false;
    }

    CloseHandle(parentInputPipe);
    CloseHandle(childStdinPipe);

    return true;
}


bool CoreTransportBase::GetUpstreamProxySettings(
    bool firstServer,
    const SessionInfo& sessionInfo,
    SystemProxySettings* systemProxySettings,
    tstring& o_UserSSHParentProxyType,
    tstring& o_UserSSHParentProxyHostname,
    int& o_UserSSHParentProxyPort,
    tstring& o_UserSSHParentProxyUsername,
    tstring& o_UserSSHParentProxyPassword)
{
    o_UserSSHParentProxyType.clear();
    o_UserSSHParentProxyHostname.clear();
    o_UserSSHParentProxyUsername.clear();
    o_UserSSHParentProxyPassword.clear();
    o_UserSSHParentProxyPort = 0;

    //Check if user wants to use parent proxy
    if (UserSkipSSHParentProxySettings())
    {
        /*
        // Embedded http in-proxies
        // NOTE: this feature breaks split tunnelling since the server will resolve
        // the geolocation of the client as the in-proxy's location
        vector<tstring> proxyIpAddresses;
        
        bool useProxy = !proxyIpAddresses.empty() && (rand() % 2 == 0);

        if (useProxy && !firstServer)
        {
            o_UserSSHParentProxyType = _T("https");
            o_UserSSHParentProxyUsername = _T("user");
            o_UserSSHParentProxyPassword = _T("password");
            o_UserSSHParentProxyPort = 3128;

            random_shuffle(proxyIpAddresses.begin(), proxyIpAddresses.end());
            o_UserSSHParentProxyHostname = proxyIpAddresses.at(0);

            ostringstream ss;
            string hostnameWithDashes = TStringToNarrow(o_UserSSHParentProxyHostname);
            std::replace(hostnameWithDashes.begin(), hostnameWithDashes.end(), '.', '-');
            ss << "{ipAddress: " << sessionInfo.GetServerAddress() << ", throughProxy: " << hostnameWithDashes << "}";
            AddDiagnosticInfoYaml("ProxiedConnection", ss.str().c_str());
            return true;
        }
        */
        return false;
    }
    //Registry values take precedence over system settings
    //Username and password for 'Basic' HTTP or SOCKS authentication
    //must be stored in registry

    o_UserSSHParentProxyType = NarrowToTString(UserSSHParentProxyType());
    o_UserSSHParentProxyHostname = NarrowToTString(UserSSHParentProxyHostname());
    o_UserSSHParentProxyPort =  UserSSHParentProxyPort();
    o_UserSSHParentProxyUsername = NarrowToTString(UserSSHParentProxyUsername());
    o_UserSSHParentProxyPassword =  NarrowToTString(UserSSHParentProxyPassword());

    if(!o_UserSSHParentProxyType.empty() 
        && !o_UserSSHParentProxyHostname.empty()
        && 0 != o_UserSSHParentProxyPort)
    {
        return true;
    }

    //if no registry values try system settings

    DecomposedProxyConfig proxyConfig;
    GetNativeDefaultProxyInfo(proxyConfig);

    if (!proxyConfig.httpsProxy.empty())
    {
        o_UserSSHParentProxyType = _T("https");
        o_UserSSHParentProxyHostname = proxyConfig.httpsProxy;
        o_UserSSHParentProxyPort = proxyConfig.httpsProxyPort;
        return true;
    }
    else if (!proxyConfig.socksProxy.empty())
    {
        o_UserSSHParentProxyType = _T("socks");
        o_UserSSHParentProxyHostname = proxyConfig.socksProxy;
        o_UserSSHParentProxyPort = proxyConfig.socksProxyPort;
        return true;
    }

    return false;
}


/******************************************************************************
 SSHTransport
******************************************************************************/

// Support the registration of this transport type
static ITransport* NewSSH()
{
    return new SSHTransport();
}


// static
void SSHTransport::GetFactory(
                    tstring& o_transportDisplayName,
                    tstring& o_transportProtocolName,
                    TransportFactoryFn& o_transportFactoryFn,
                    AddServerEntriesFn& o_addServerEntriesFn)
{
    o_transportFactoryFn = NewSSH;
    o_transportDisplayName = SSH_TRANSPORT_DISPLAY_NAME;
    o_transportProtocolName = SSH_TRANSPORT_PROTOCOL_NAME;
    // Note: these server entries are not used as the core manages its own
    // database of entries.
    // TODO: add code to harvest these entries?
    o_addServerEntriesFn = ITransport::AddServerEntries;
}


SSHTransport::SSHTransport()
    : CoreTransportBase(GetTransportProtocolName().c_str())
{
}


SSHTransport::~SSHTransport()
{
    IWorkerThread::Stop();
}


// static
tstring SSHTransport::GetTransportProtocolName() const 
{
    return SSH_TRANSPORT_PROTOCOL_NAME;
}


tstring SSHTransport::GetTransportDisplayName() const 
{ 
    return SSH_TRANSPORT_DISPLAY_NAME; 
}


tstring SSHTransport::GetTransportRequestName() const
{
    return GetTransportProtocolName();
}


/******************************************************************************
 OSSHTransport
******************************************************************************/

// Support the registration of this transport type
static ITransport* NewOSSH()
{
    return new OSSHTransport();
}


// static
void OSSHTransport::GetFactory(
                    tstring& o_transportDisplayName,
                    tstring& o_transportProtocolName,
                    TransportFactoryFn& o_transportFactory,
                    AddServerEntriesFn& o_addServerEntriesFn)
{
    o_transportFactory = NewOSSH;
    o_transportDisplayName = OSSH_TRANSPORT_DISPLAY_NAME;
    o_transportProtocolName = OSSH_TRANSPORT_PROTOCOL_NAME;
    // Note: these server entries are not used as the core manages its own
    // database of entries.
    // TODO: add code to harvest these entries?
    o_addServerEntriesFn = ITransport::AddServerEntries;
}


OSSHTransport::OSSHTransport()
    : CoreTransportBase(OSSH_TRANSPORT_PROTOCOL_NAME)
{
}


OSSHTransport::~OSSHTransport()
{
    IWorkerThread::Stop();
}


tstring OSSHTransport::GetTransportProtocolName() const 
{
    return OSSH_TRANSPORT_PROTOCOL_NAME;
}


tstring OSSHTransport::GetTransportDisplayName() const 
{
    return OSSH_TRANSPORT_DISPLAY_NAME;
}


tstring OSSHTransport::GetTransportRequestName() const
{
    // Should not be called for core as there should be no Psiphon API web requests outside of the core.
    assert(false);
    return GetTransportProtocolName();
}
