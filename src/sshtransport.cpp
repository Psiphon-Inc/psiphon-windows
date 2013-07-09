/*
 * Copyright (c) 2013, Psiphon Inc.
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
#include "sessioninfo.h"
#include "psiclient.h"
#include <WinSock2.h>
#include <WinCrypt.h>
#include "utilities.h"
#include "systemproxysettings.h"
#include "config.h"


#define DEFAULT_PLONK_SOCKS_PROXY_PORT  1080
#define SSH_CONNECTION_TIMEOUT_SECONDS  20
#define PLONK_EXE_NAME                  _T("psiphon3-plonk.exe")


static bool SetPlonkSSHHostKey(
        const tstring& sshServerAddress,
        int sshServerPort,
        const tstring& sshServerHostKey);



class PlonkConnection
{
    /*
    0s ... fresh ... 30s ... retired ... 50s kill
    */

public:
    PlonkConnection(const SessionInfo& sessionInfo);
    virtual ~PlonkConnection();

    bool IsInitialized() const;
    bool IsOkay() const;
    bool InFreshEra() const;
    bool InRetiredEra() const;
    bool InKillEra() const;

    void Kill();

    bool Connect(
        int localSocksProxyPort,
        LPCTSTR serverAddress, 
        LPCTSTR serverHostKey, 
        LPCTSTR plonkPath, 
        LPCTSTR plonkCommandLine,
        int serverPort,
        const StopInfo& stopInfo);

    bool CheckForConnected();

    void StopPortFoward();

protected:
    DWORD GetFreshLimit() const;
    DWORD GetRetiredLimit() const;

private:
    PROCESS_INFORMATION m_processInfo;
    DWORD m_startTick;
    const SessionInfo& m_sessionInfo;
    const StopInfo* m_stopInfo;
    HANDLE m_plonkInputHandle;
    HANDLE m_plonkOutputHandle;
};


/******************************************************************************
 SSHTransportBase
******************************************************************************/

SSHTransportBase::SSHTransportBase()
{
    m_localSocksProxyPort = DEFAULT_PLONK_SOCKS_PROXY_PORT;
    m_serverPort = 0;
}

SSHTransportBase::~SSHTransportBase()
{
    (void)Cleanup();
}

bool SSHTransportBase::IsServerRequestTunnelled() const
{
    return true;
}

bool SSHTransportBase::IsSplitTunnelSupported() const
{
    return true;
}

bool SSHTransportBase::IsMultiConnectSupported() const
{
    return true;
}

bool SSHTransportBase::ServerHasCapabilities(const ServerEntry& entry) const
{
    return entry.HasCapability(TStringToNarrow(GetTransportProtocolName()));
}

tstring SSHTransportBase::GetSessionID(const SessionInfo& sessionInfo)
{
    return NarrowToTString(sessionInfo.GetSSHSessionID());
}

int SSHTransportBase::GetLocalProxyParentPort() const
{
    return m_localSocksProxyPort;
}

tstring SSHTransportBase::GetLastTransportError() const
{
    return _T("0");
}

bool SSHTransportBase::DoPeriodicCheck()
{
    // Make sure the current connection is okay.
    if (m_currentPlonk.get() == NULL 
        || !m_currentPlonk->IsInitialized()
        || !m_currentPlonk->IsOkay())
    {
        // Either the current connection was never created, or it has been lost.
        // Either way, fail.
        my_print(NOT_SENSITIVE, true, _T("%s: m_currentPlonk is not okay"), __TFUNCTION__);
        return false;
    }

    if (m_currentPlonk->InFreshEra())
    {
        // Previous connection may exist in retired state, and may need to be killed.
        if (m_previousPlonk.get() != NULL
            && m_previousPlonk->InKillEra())
        {
            my_print(NOT_SENSITIVE, true, _T("%s: m_previousPlonk is in kill era, killing it"), __TFUNCTION__);

            m_previousPlonk->Kill();
            m_previousPlonk.reset();

            if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
            {
                throw Abort();
            }
        }

        return true;
    }
    else if (m_currentPlonk->InRetiredEra()
             || m_currentPlonk->InKillEra())
    {
        // It shouldn't happen that the current connection goes into the kill
        // era, but it could, in theory, if things get really bad/slow.
        assert(!m_currentPlonk->InKillEra());

        my_print(NOT_SENSITIVE, true, _T("%s: m_currentPlonk is in retired era, retiring it"), __TFUNCTION__);

        // The previous connection should not exist, but if we made our time
        // limits too tight, it might. If it does exist, kill it.
        if (m_previousPlonk.get() != NULL)
        {
            m_previousPlonk->Kill();
            m_previousPlonk.reset();

            if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
            {
                throw Abort();
            }
        }

        // Time to bring up the next connection and retire the current.

        auto_ptr<PlonkConnection> nextPlonk(new PlonkConnection(m_sessionInfo));

        // TODO: Check for out-of-memory allocation failure

        // We assume that TransportConnectHelper has already been called, so 
        // the Plonk executable and server information are initialized.

        bool connectSuccess = nextPlonk->Connect(
                                        m_localSocksProxyPort,
                                        m_serverAddress.c_str(),
                                        m_serverHostKey.c_str(),
                                        m_plonkPath.c_str(),
                                        m_plonkCommandLine.c_str(),
                                        m_serverPort,
                                        m_stopInfo);

        if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
        {
            throw Abort();
        }

        if (connectSuccess)
        {
            m_previousPlonk = m_currentPlonk;
            m_currentPlonk = nextPlonk;
            
            // Cause the previous Plonk to stop listening locally, so the new
            // Plonk can handle new connection. But we leave the old one running
            // so that it can fulfill outstanding requests.
            m_previousPlonk->StopPortFoward();
        }
        else
        {
            my_print(NOT_SENSITIVE, true, _T("%s: next plonk connect failed"), __TFUNCTION__);
        }
        // If next plonk connection failed, try again next time.
    }

    // It might be time to kill the previous, retired connection.
    if (m_previousPlonk.get() != NULL)
    {
        if (m_previousPlonk->InKillEra())
        {
            my_print(NOT_SENSITIVE, true, _T("%s: m_previousPlonk is in kill era, killing it"), __TFUNCTION__);

            m_previousPlonk->Kill();
            m_previousPlonk.reset();

            if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
            {
                throw Abort();
            }
        }
    }

    return true;
}

bool SSHTransportBase::Cleanup()
{
    if (m_previousPlonk.get() != NULL)
    {
        m_previousPlonk->Kill();
        m_previousPlonk.reset();
    }

    if (m_currentPlonk.get() != NULL)
    {
        m_currentPlonk->Kill();
        m_currentPlonk.reset();
    }

    return true;
}

void SSHTransportBase::TransportConnect()
{
    if (!AreAnyServersSSHCapable())
    {
        throw TransportFailed();
    }

    try
    {
        TransportConnectHelper();
    }
    catch(...)
    {
        (void)Cleanup();
        throw;
    }
}

void SSHTransportBase::TransportConnectHelper()
{
    my_print(NOT_SENSITIVE, false, _T("%s connecting..."), GetTransportDisplayName().c_str());

    // Extract executables and put to disk if not already

    if (m_plonkPath.size() == 0)
    {
        if (!ExtractExecutable(IDR_PLONK_EXE, PLONK_EXE_NAME, m_plonkPath))
        {
            throw TransportFailed();
        }
    }

    /*
    We will be trying to make multiple SSH connections to different servers at 
    the same time. They will all be listening as SOCKS proxies on the same local
    port -- this will work due to the SO_REUSEADDR flag.
    */

    // Test if the localSocksProxyPort is already in use.  If it is, try to find
    // one that is available.
    m_localSocksProxyPort = DEFAULT_PLONK_SOCKS_PROXY_PORT;
    if (!TestForOpenPort(m_localSocksProxyPort, 10, m_stopInfo))
    {
        my_print(NOT_SENSITIVE, false, _T("Local SOCKS proxy could not find an available port."));
        throw TransportFailed();
    }

    vector<SessionInfo>::const_iterator sessionInfo = m_sessionInfo.begin();
    assert(sessionInfo != m_sessionInfo.end());

    *** Loop through sessioninfo items, trying to start them.
    *** For server stickiness, give the first one a head-start.

    // Start plonk using Psiphon server SSH parameters

    if (!GetSSHParams(
        sessionInfo,
        m_localSocksProxyPort,
        m_serverAddress, 
        m_serverPort, 
        m_serverHostKey, 
        m_plonkCommandLine, 
        systemProxySettings))
    {
        throw TransportFailed();
    }

    m_currentPlonk.reset(new PlonkConnection(m_sessionInfo));

    // TODO: Check for out-of-memory allocation failure

    bool connectSuccess = m_currentPlonk->Connect(
                                    m_localSocksProxyPort,
                                    m_serverAddress.c_str(),
                                    m_serverHostKey.c_str(),
                                    m_plonkPath.c_str(),
                                    m_plonkCommandLine.c_str(),
                                    m_serverPort,
                                    m_stopInfo);

    if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
    {
        throw Abort();
    }
    else if (!connectSuccess)
    {
        throw TransportFailed();
    }

    systemProxySettings->SetSocksProxyPort(m_localSocksProxyPort);

    my_print(NOT_SENSITIVE, false, _T("SOCKS proxy is running on localhost port %d."), m_localSocksProxyPort);
}

bool SSHTransportBase::AreAnyServersSSHCapable()
{
    // Reverse through vector, so we can remove.
    for (size_t i = m_sessionInfo.size()-1; i >= 0; i--)
    {
        if (m_sessionInfo[i].GetSSHHostKey().length() <= 0)
        {
            m_sessionInfo.erase(m_sessionInfo.begin()+i);
        }
    }

    return m_sessionInfo.size() > 0;
}

bool SSHTransportBase::GetUserParentProxySettings(
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
    if(UserSkipSSHParentProxySettings())
    {
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
    return(systemProxySettings->GetUserLanProxy(
        o_UserSSHParentProxyType, 
        o_UserSSHParentProxyHostname, 
        o_UserSSHParentProxyPort));
}

bool SSHTransportBase::GetSSHParams(
    const SessionInfo& sessionInfo,
    const int localSocksProxyPort,
    tstring& o_serverAddress, 
    int& o_serverPort, 
    tstring& o_serverHostKey, 
    tstring& o_plonkCommandLine,
    SystemProxySettings* systemProxySettings)
{
    o_serverAddress.clear();
    o_serverPort = 0;
    o_serverHostKey.clear();
    o_plonkCommandLine.clear();

    o_serverAddress = NarrowToTString(sessionInfo.GetServerAddress());
    o_serverPort = GetPort(sessionInfo);
    o_serverHostKey = NarrowToTString(sessionInfo.GetSSHHostKey());

    // Client transmits its session ID prepended to the SSH password; the server
    // uses this to associate the tunnel with web requests -- for GeoIP region stats
    string sshPassword = sessionInfo.GetClientSessionID() + sessionInfo.GetSSHPassword();

    // Note: -batch ensures plonk doesn't hang on a prompt when the server's host key isn't
    // the expected value we just set in the registry

    tstringstream args;
    args << _T(" -ssh -C -N -batch")
         << _T(" -P ") << o_serverPort
         << _T(" -l ") << NarrowToTString(sessionInfo.GetSSHUsername()).c_str()
         << _T(" -pw ") << NarrowToTString(sshPassword).c_str()
         << _T(" -D ") << localSocksProxyPort;

    // Now using this flag for debug and release. We use the verbose Plonk 
    // output to determine when it has successfully connected.
    args << _T(" -v");

    tstring proxy_type, proxy_host, proxy_username, proxy_password;
    int proxy_port;

    if(GetUserParentProxySettings(
        systemProxySettings, 
        proxy_type, 
        proxy_host, 
        proxy_port, 
        proxy_username, 
        proxy_password))
    {
        args << _T(" -proxy_type ") << proxy_type.c_str();
        args << _T(" -proxy_host ") << proxy_host.c_str();
        args << _T(" -proxy_port ") << proxy_port;
        if(!proxy_username.empty())
        {
            args << _T(" -proxy_username ") << proxy_username.c_str();
        }
        if(!proxy_password.empty())
        {
            args << _T(" -proxy_password ") << proxy_password.c_str();
        }

    }
    
    o_plonkCommandLine = m_plonkPath + args.str();
    return true;
}


/******************************************************************************
 PlonkConnection
******************************************************************************/

PlonkConnection::PlonkConnection(const SessionInfo& sessionInfo)
    : m_startTick(0),
      m_sessionInfo(sessionInfo),
      m_stopInfo(NULL),
      m_plonkInputHandle(INVALID_HANDLE_VALUE),
      m_plonkOutputHandle(INVALID_HANDLE_VALUE)
{
    ZeroMemory(&m_processInfo, sizeof(m_processInfo));
}

PlonkConnection::~PlonkConnection()
{
    Kill();
}

bool PlonkConnection::IsInitialized() const
{
    return m_processInfo.hProcess != 0;
}

bool PlonkConnection::IsOkay() const
{
    DWORD result = WaitForSingleObject(m_processInfo.hProcess, 0);

    if (result == WAIT_TIMEOUT)
    {
        // Everything normal
        return true;
    }
    else if (result == WAIT_OBJECT_0)
    {
        // The process has signalled -- which implies that it has died
        return false;
    }

    std::stringstream s;
    s << __FUNCTION__ << ": WaitForSingleObject failed (" << result << ", " << GetLastError() << ")";
    throw IWorkerThread::Error(s.str().c_str());
    return false;
}

DWORD PlonkConnection::GetFreshLimit() const
{
    // This initialized value should never be used, but just to be safe...
    DWORD freshLimit = MAXDWORD;

    // If there's a registry value, it will override the value from the handshake.
    if (!ReadRegistryDwordValue(string("SSHReconnectFreshLimit"), freshLimit))
    {
        if (m_sessionInfo.GetPreemptiveReconnectLifetimeMilliseconds() == 0)
        {
            // Functionality is disabled
            freshLimit = MAXDWORD;
        }
        else
        {
            freshLimit = m_sessionInfo.GetPreemptiveReconnectLifetimeMilliseconds() / 2;
        }
    }

    static DWORD s_loggedLimit = 0;
    if (s_loggedLimit != freshLimit)
    {
        s_loggedLimit = freshLimit;
        my_print(NOT_SENSITIVE, true, _T("%s: Fresh limit: %u"), __TFUNCTION__, freshLimit);
    }

    return freshLimit;
}

DWORD PlonkConnection::GetRetiredLimit() const
{
    // This initialized value should never be used, but just to be safe...
    DWORD retiredLimit = MAXDWORD;

    // If there's a registry value, it will override the value from the handshake.
    if (!ReadRegistryDwordValue(string("SSHReconnectRetiredLimit"), retiredLimit))
    {
        if (m_sessionInfo.GetPreemptiveReconnectLifetimeMilliseconds() == 0)
        {
            // Functionality is disabled
            retiredLimit = MAXDWORD;
        }
        else
        {
            assert(m_sessionInfo.GetPreemptiveReconnectLifetimeMilliseconds() > 10000);
            retiredLimit = m_sessionInfo.GetPreemptiveReconnectLifetimeMilliseconds() - 10000;
        }
    }

    static DWORD s_loggedLimit = 0;
    if (s_loggedLimit != retiredLimit)
    {
        s_loggedLimit = retiredLimit;
        my_print(NOT_SENSITIVE, true, _T("%s: Retired limit: %u"), __TFUNCTION__, retiredLimit);
    }

    return retiredLimit;
}

bool PlonkConnection::InFreshEra() const
{
    DWORD age = GetTickCountDiff(m_startTick, GetTickCount());

    return age > 0 && age < GetFreshLimit();
}

bool PlonkConnection::InRetiredEra() const
{
    // TODO: Check if really in not-listening state?
    // If so, probably need to change other era checks.

    DWORD age = GetTickCountDiff(m_startTick, GetTickCount());

    return age >= GetFreshLimit() && age < GetRetiredLimit();
}

bool PlonkConnection::InKillEra() const
{
    DWORD age = GetTickCountDiff(m_startTick, GetTickCount());

    return age >= GetRetiredLimit();
}

void PlonkConnection::Kill()
{
    m_startTick = 0;
    m_stopInfo = NULL;

    if (m_plonkInputHandle != INVALID_HANDLE_VALUE) CloseHandle(m_plonkInputHandle);
    m_plonkInputHandle = INVALID_HANDLE_VALUE;

    if (m_plonkOutputHandle != INVALID_HANDLE_VALUE) CloseHandle(m_plonkOutputHandle);
    m_plonkOutputHandle = INVALID_HANDLE_VALUE;

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
}

bool PlonkConnection::Connect(
        int localSocksProxyPort,
        LPCTSTR serverAddress, 
        LPCTSTR serverHostKey, 
        LPCTSTR plonkPath, 
        LPCTSTR plonkCommandLine,
        int serverPort,
        const StopInfo& stopInfo)
{
    // Ensure we start from a disconnected/clean state
    Kill();

    m_stopInfo = &stopInfo;
    m_startTick = GetTickCount();

    // Add host to Plonk's known host registry set
    // Note: currently we're not removing this after the session, so we're leaving a trace

    if (!SetPlonkSSHHostKey(serverAddress, serverPort, serverHostKey))
    {
        return false;
    }

    // Create the Plonk process and connect to server
    STARTUPINFO plonkStartupInfo;
    ZeroMemory(&plonkStartupInfo, sizeof(plonkStartupInfo));
    plonkStartupInfo.cb = sizeof(plonkStartupInfo);

    // We'll read from this to determine when Plonk has connected
    HANDLE plonkOutput = INVALID_HANDLE_VALUE;

    // We'll write to this to tell Plonk when to shut down its port forwarder.
    HANDLE plonkInput = INVALID_HANDLE_VALUE;

    plonkStartupInfo.dwFlags = STARTF_USESTDHANDLES;

    if (!CreateSubprocessPipes(
            plonkOutput,
            plonkInput,
            plonkStartupInfo.hStdInput,
            plonkStartupInfo.hStdOutput, 
            plonkStartupInfo.hStdError))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - CreatePolipoPipe failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    if (!CreateProcess(
            plonkPath,
            (TCHAR*)plonkCommandLine,
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
            &plonkStartupInfo,
            &m_processInfo))
    {
        CloseHandle(plonkOutput);
        CloseHandle(plonkInput);
        CloseHandle(plonkStartupInfo.hStdInput);
        CloseHandle(plonkStartupInfo.hStdOutput);
        CloseHandle(plonkStartupInfo.hStdError);
        return false;
    }

    // Close the unneccesary handles
    CloseHandle(m_processInfo.hThread);
    m_processInfo.hThread = NULL;

    // Close child pipe handle (do not continue to modify the parent).
    // You need to make sure that no handles to the write end of the
    // output pipe are maintained in this process or else the pipe will
    // not close when the child process exits and the ReadFile will hang.
    if (!CloseHandle(plonkStartupInfo.hStdOutput))
    {
        CloseHandle(plonkOutput);
        CloseHandle(plonkInput);
        CloseHandle(plonkStartupInfo.hStdInput);
        CloseHandle(plonkStartupInfo.hStdError);
        my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
    }
    if (!CloseHandle(plonkStartupInfo.hStdError))
    {
        CloseHandle(plonkOutput);
        CloseHandle(plonkInput);
        CloseHandle(plonkStartupInfo.hStdInput);
        my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
    }
    if (!CloseHandle(plonkStartupInfo.hStdInput))
    {
        CloseHandle(plonkOutput);
        CloseHandle(plonkInput);
        my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
    }

    if (m_stopInfo->stopSignal->CheckSignal(m_stopInfo->stopReasons))
    {
        CloseHandle(plonkOutput);
        CloseHandle(plonkInput);
        return false;
    }

    WaitForInputIdle(m_processInfo.hProcess, 5000);

    if (m_stopInfo->stopSignal->CheckSignal(m_stopInfo->stopReasons))
    {
        CloseHandle(plonkOutput);
        CloseHandle(plonkInput);
        return false;
    }

    m_plonkInputHandle = plonkInput;
    m_plonkOutputHandle = plonkOutput;

    // The caller is responsible for waiting for CheckForConnected to be true

    return true;
}


// Has the side-effect of closing the Plonk output handle when connected.
bool PlonkConnection::CheckForConnected()
{
    assert(m_stopInfo != NULL);
    assert(m_plonkOutputHandle != INVALID_HANDLE_VALUE);

    SetLastError(ERROR_SUCCESS);

    if (m_stopInfo->stopSignal->CheckSignal(m_stopInfo->stopReasons))
    {
        my_print(NOT_SENSITIVE, true, _T("%s:%d - Stop signaled"), __TFUNCTION__, __LINE__);
        return false;
    }

    DWORD bytes_avail = 0;

    // ReadFile will block forever if there's no data to read, so we need
    // to check if there's data available to read first.
    if (!PeekNamedPipe(m_plonkOutputHandle, NULL, 0, NULL, &bytes_avail, NULL))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - PeekNamedPipe failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
        return false;
    }

    // If there's data available from the Plonk pipe, process it.
    if (bytes_avail > 0)
    {
        char* buffer = new char[bytes_avail+1];
        DWORD num_read = 0;
        if (!ReadFile(m_plonkOutputHandle, buffer, bytes_avail, &num_read, NULL))
        {
            my_print(NOT_SENSITIVE, false, _T("%s:%d - ReadFile failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
            false;
        }
        buffer[bytes_avail] = '\0';

        // Note that we are only capturing Plonk output during the connect sequence.
        my_print(NOT_SENSITIVE, true, _T("%s:%d Plonk output: >>>%S<<<"), __TFUNCTION__, __LINE__, buffer);

        bool connected = (strstr(buffer, "PSIPHON:CONNECTED") != NULL);

        delete[] buffer;

        if (connected)
        {
            // We're done reading Plonk output
            if (!CloseHandle(m_plonkOutputHandle))
            {
                my_print(NOT_SENSITIVE, false, _T("%s:%d - CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
                return false;
            }
            m_plonkOutputHandle = INVALID_HANDLE_VALUE;

            //my_print(LogSensitivity::NOT_SENSITIVE, true, "SSH connect SUCCESS");
            return true;
        }
    }

    return false;
}


void PlonkConnection::StopPortFoward()
{
    if (m_plonkInputHandle == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD bytesWritten = 0;

    LPCSTR stopPortfwd = "PSIPHON:PORTFWDSTOP";
    BOOL success = WriteFile(
                        m_plonkInputHandle, 
                        stopPortfwd,
                        strlen(stopPortfwd),
                        &bytesWritten,
                        NULL);

    assert(success);
    assert(bytesWritten == strlen(stopPortfwd));

    CloseHandle(m_plonkInputHandle);
    m_plonkInputHandle = INVALID_HANDLE_VALUE;
}


/******************************************************************************
 SSHTransport
******************************************************************************/

static const TCHAR* SSH_TRANSPORT_PROTOCOL_NAME = _T("SSH");
static const TCHAR* SSH_TRANSPORT_DISPLAY_NAME = _T("SSH");

// Support the registration of this transport type
static ITransport* NewSSH()
{
    return new SSHTransport();
}

// static
void SSHTransport::GetFactory(
                    tstring& o_transportName,
                    TransportFactory& o_transportFactory)
{
    o_transportFactory = NewSSH;
    o_transportName = SSH_TRANSPORT_DISPLAY_NAME;
}


SSHTransport::SSHTransport()
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

bool SSHTransport::IsHandshakeRequired(const ServerEntry& entry) const
{
    bool sufficientInfo = 
        entry.serverAddress.length() > 0
        && entry.sshPort > 0
        && entry.sshHostKey.length() > 0
        && entry.sshUsername.length() > 0
        && entry.sshPassword.length() > 0;
    return !sufficientInfo;
}

bool SSHTransport::GetSSHParams(
    const SessionInfo& sessionInfo,
    const int localSocksProxyPort,
    const string& sshPassword,
    tstring& o_serverAddress, 
    int& o_serverPort, 
    tstring& o_serverHostKey, 
    tstring& o_plonkCommandLine,
    SystemProxySettings* systemProxySettings)
{
    tstring o_plonk_options;

    if(!SSHTransportBase::GetSSHParams(
        sessionInfo,
        localSocksProxyPort,
        sshPassword,
        o_serverAddress, 
        o_serverPort, 
        o_serverHostKey, 
        o_plonk_options,
        systemProxySettings))
    {
        return false;
    }
    tstringstream args;
    args << o_plonk_options << _T(" ") << o_serverAddress;
    o_plonkCommandLine = args.str();

    return true;
}

int SSHTransport::GetPort(const SessionInfo& sessionInfo) const
{
    return sessionInfo.GetSSHPort();
}


/******************************************************************************
 OSSHTransport
******************************************************************************/

static const TCHAR* OSSH_TRANSPORT_PROTOCOL_NAME = _T("OSSH");
static const TCHAR* OSSH_TRANSPORT_DISPLAY_NAME = _T("SSH+");

// Support the registration of this transport type
static ITransport* NewOSSH()
{
    return new OSSHTransport();
}

// static
void OSSHTransport::GetFactory(
                    tstring& o_transportName,
                    TransportFactory& o_transportFactory)
{
    o_transportFactory = NewOSSH;
    o_transportName = OSSH_TRANSPORT_DISPLAY_NAME;
}


OSSHTransport::OSSHTransport()
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

bool OSSHTransport::IsHandshakeRequired(const ServerEntry& entry) const
{
    bool sufficientInfo = 
        entry.serverAddress.length() > 0
        && entry.sshObfuscatedPort > 0
        && entry.sshHostKey.length() > 0
        && entry.sshUsername.length() > 0
        && entry.sshPassword.length() > 0
        && entry.sshObfuscatedKey.length() > 0;
    return !sufficientInfo;
}

bool OSSHTransport::GetSSHParams(
    const SessionInfo& sessionInfo,
    const int localSocksProxyPort,
    const string& sshPassword,
    tstring& o_serverAddress, 
    int& o_serverPort, 
    tstring& o_serverHostKey, 
    tstring& o_plonkCommandLine,
    SystemProxySettings* systemProxySettings)
{

    if (sessionInfo.GetSSHObfuscatedPort() <= 0 
        || sessionInfo.GetSSHObfuscatedKey().size() <= 0)
    {
        my_print(NOT_SENSITIVE, false, _T("%s - missing parameters"), __TFUNCTION__);
        return false;
    }

    tstring o_plonk_options;

    if(!SSHTransportBase::GetSSHParams(
        sessionInfo,
        localSocksProxyPort,
        sshPassword,
        o_serverAddress, 
        o_serverPort, 
        o_serverHostKey, 
        o_plonk_options,
        systemProxySettings))
    {
        return false;
    }

    tstringstream args;

    args << o_plonk_options; 
    args << _T(" -z -Z ") << NarrowToTString(sessionInfo.GetSSHObfuscatedKey()).c_str();
    args << _T(" ") << o_serverAddress.c_str();
    o_plonkCommandLine = args.str();

    return true;
}

int OSSHTransport::GetPort(const SessionInfo& sessionInfo) const
{
    return sessionInfo.GetSSHObfuscatedPort();
}


/******************************************************************************
 Helpers
******************************************************************************/

bool SetPlonkSSHHostKey(
        const tstring& sshServerAddress,
        int sshServerPort,
        const tstring& sshServerHostKey)
{
    // Add Plonk registry entry for host for non-interactive host key validation

    // Host key is base64 encoded set of fields

    BYTE* decodedFields = NULL;
    DWORD size = 0;

    if (!CryptStringToBinary(sshServerHostKey.c_str(), sshServerHostKey.length(), CRYPT_STRING_BASE64, NULL, &size, NULL, NULL)
        || !(decodedFields = new (std::nothrow) BYTE[size])
        || !CryptStringToBinary(sshServerHostKey.c_str(), sshServerHostKey.length(), CRYPT_STRING_BASE64, decodedFields, &size, NULL, NULL))
    {
        my_print(NOT_SENSITIVE, false, _T("SetPlonkSSHHostKey: CryptStringToBinary failed (%d)"), GetLastError());
        return false;
    }

    // field format: {<4 byte len (big endian), len bytes field>}+
    // first field is key type, expecting "ssh-rsa";
    // remaining fields are opaque number value -- simply emit in new format which is comma delimited hex strings

    const char* expectedKeyTypeValue = "ssh-rsa";
    unsigned long expectedKeyTypeLen = htonl(strlen(expectedKeyTypeValue));

    if (memcmp(decodedFields + 0, &expectedKeyTypeLen, sizeof(unsigned long))
        || memcmp(decodedFields + sizeof(unsigned long), expectedKeyTypeValue, strlen(expectedKeyTypeValue)))
    {
        delete [] decodedFields;

        my_print(NOT_SENSITIVE, false, _T("SetPlonkSSHHostKey: unexpected key type"));
        return false;
    }

    string data;

    unsigned long offset = sizeof(unsigned long) + strlen(expectedKeyTypeValue);

    while (offset < size - sizeof(unsigned long))
    {
        unsigned long nextLen = ntohl(*((long*)(decodedFields + offset)));
        offset += sizeof(unsigned long);

        if (nextLen > 0 && offset + nextLen <= size)        
        {
            string field = "";
            const char* hexDigits = "0123456789abcdef";
            for (unsigned long i = 0; i < nextLen; i++)
            {
                char digit = hexDigits[decodedFields[offset + i] >> 4];
                // Don't add leading zeroes
                if (digit != '0' || field.length() > 0) field += digit;
                digit = hexDigits[decodedFields[offset + i] & 0x0F];
                // Always include last nibble (e.g. 0x0)
                if (i == nextLen-1 || (digit != '0' || field.length() > 0)) field += digit;
            }
            field = "0x" + field;
            if (data.length() > 0) data += ",";
            data += field;
            offset += nextLen;
        }
    }

    delete [] decodedFields;

    stringstream value;
    value << "rsa2@" << sshServerPort << ":" << TStringToNarrow(sshServerAddress);

    const TCHAR* plonkRegistryKey = _T("Software\\SimonTatham\\PuTTY\\SshHostKeys");

    HKEY key = 0;
    LONG returnCode = RegCreateKeyEx(HKEY_CURRENT_USER, plonkRegistryKey, 0, 0, 0, KEY_WRITE, 0, &key, NULL);
    if (ERROR_SUCCESS != returnCode)
    {
        my_print(NOT_SENSITIVE, false, _T("SetPlonkSSHHostKey: Create Registry Key failed (%d)"), returnCode);
        return false;
    }

    returnCode = RegSetValueExA(key, value.str().c_str(), 0, REG_SZ, (PBYTE)data.c_str(), data.length()+1);
    if (ERROR_SUCCESS != returnCode)
    {
        RegCloseKey(key);

        my_print(NOT_SENSITIVE, false, _T("SetPlonkSSHHostKey: Set Registry Value failed (%d)"), returnCode);
        return false;
    }

    RegCloseKey(key);

    return true;
}

