/*
 * Copyright (c) 2014, Psiphon Inc.
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
#include "diagnostic_info.h"
#include <stdlib.h>
#include <time.h>


#define DEFAULT_PLONK_SOCKS_PROXY_PORT  1080
#define SSH_CONNECTION_TIMEOUT_SECONDS  20
#define PLONK_EXE_NAME                  _T("psiphon3-plonk.exe")
#define CAPABILITY_UNFRONTED_MEEK       "UNFRONTED-MEEK"
#define CAPABILITY_FRONTED_MEEK         "FRONTED-MEEK"

// TODO: Should this value be based on the performance/resources of the system?
#define MULTI_CONNECT_POOL_SIZE         10
#define SERVER_AFFINITY_HEAD_START_MS   500

static const TCHAR* SSH_TRANSPORT_PROTOCOL_NAME = _T("SSH");
static const TCHAR* SSH_TRANSPORT_DISPLAY_NAME = _T("SSH");

static const TCHAR* OSSH_TRANSPORT_PROTOCOL_NAME = _T("OSSH");
static const TCHAR* OSSH_TRANSPORT_DISPLAY_NAME = _T("SSH+");

static const TCHAR* SSH_REQUEST_PROTOCOL_NAME = SSH_TRANSPORT_PROTOCOL_NAME;
static const TCHAR* OSSH_REQUEST_PROTOCOL_NAME = OSSH_TRANSPORT_PROTOCOL_NAME;
static const TCHAR* UNFRONTED_MEEK_REQUEST_PROTOCOL_NAME = _T("UNFRONTED-MEEK-OSSH");
static const TCHAR* FRONTED_MEEK_REQUEST_PROTOCOL_NAME = _T("FRONTED-MEEK-OSSH");

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
        LPCTSTR transportRequestName,
        const StopInfo& stopInfo);

    bool CheckForConnected(bool& o_connected);

    void StopPortFoward();

    void GetConnectParams(
        int& o_localSocksProxyPort,
        tstring& o_serverAddress, 
        tstring& o_serverHostKey, 
        tstring& o_plonkPath, 
        tstring& o_plonkCommandLine,
        int& o_serverPort,
        tstring& o_transportRequestName) const;

    // A convenience subset of GetConnectParams
    void GetTransportInfo(tstring& transportRequestName, tstring& serverAddress) const;

    // PlonkConnection keeps its own copy of SessionInfo, so it needs to be
    // updated after more info is added from the handshake.
    void UpdateSessionInfo(const SessionInfo& sessionInfo);

protected:
    DWORD GetFreshLimit() const;
    DWORD GetRetiredLimit() const;

private:
    PROCESS_INFORMATION m_processInfo;
    DWORD m_startTick;
    SessionInfo m_sessionInfo;
    const StopInfo* m_stopInfo;
    HANDLE m_plonkInputHandle;
    HANDLE m_plonkOutputHandle;
    bool m_connected;

    int m_localSocksProxyPort;
    tstring m_serverAddress; 
    tstring m_serverHostKey; 
    tstring m_plonkPath; 
    tstring m_plonkCommandLine;
    tstring m_transportRequestName;
    int m_serverPort;
};


/******************************************************************************
 SSHTransportBase
******************************************************************************/

SSHTransportBase::SSHTransportBase(LPCTSTR transportProtocolName)
    : ITransport(transportProtocolName),
      m_localSocksProxyPort(DEFAULT_PLONK_SOCKS_PROXY_PORT),
      m_meekClient(NULL),
      m_meekClientStopSignal(NULL),
      m_meekClientStopInfo(NULL)
{
}

SSHTransportBase::~SSHTransportBase()
{
    (void)Cleanup();
}

bool SSHTransportBase::IsHandshakeRequired() const
{
    return false;
}

bool SSHTransportBase::IsServerRequestTunnelled() const
{
    return true;
}

bool SSHTransportBase::IsSplitTunnelSupported() const
{
    return true;
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

void SSHTransportBase::ProxySetupComplete()
{
    // Don't do a handshake if this is a temporary connection
    if (m_tempConnectServerEntry)
    {
        return;
    }

    // Do the handshake, but don't abort if it fails
    (void)DoHandshake(
            false,  // not pre-handshake
            m_sessionInfo);
    
    m_currentPlonk->UpdateSessionInfo(m_sessionInfo);
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

        boost::shared_ptr<PlonkConnection> nextPlonk(new PlonkConnection(m_sessionInfo));

        if (nextPlonk.get() == NULL)
        {
            stringstream error;
            error << __FUNCTION__ << ":" << __LINE__ << ": Out of memory";
            throw std::exception(error.str().c_str());
        }

        // We assume that TransportConnectHelper has already been called, so 
        // the Plonk executable and server information are initialized.

        int localSocksProxyPort, serverPort;
        tstring serverAddress, serverHostKey, plonkPath, plonkCommandLine, transportRequestName;

        m_currentPlonk->GetConnectParams(
            localSocksProxyPort,
            serverAddress, 
            serverHostKey, 
            plonkPath, 
            plonkCommandLine,
            serverPort,
            transportRequestName);

        assert(localSocksProxyPort == m_localSocksProxyPort);
        assert(plonkPath == m_plonkPath);

        bool connectSuccess = false, connectionComplete = false;

        connectSuccess = nextPlonk->Connect(
                                        localSocksProxyPort,
                                        serverAddress.c_str(),
                                        serverHostKey.c_str(),
                                        plonkPath.c_str(),
                                        plonkCommandLine.c_str(),
                                        serverPort,
                                        transportRequestName.c_str(),
                                        m_stopInfo);

        if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
        {
            throw Abort();
        }

        if (connectSuccess)
        {
            connectionComplete = false;
            
            DWORD startTime = GetTickCount();

            while (nextPlonk->CheckForConnected(connectionComplete)
                   && !connectionComplete
                   && GetTickCountDiff(startTime, GetTickCount()) < SSH_CONNECTION_TIMEOUT_SECONDS*1000)
            {
                if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
                {
                    throw Abort();
                }

                Sleep(100);
            }

            if (connectionComplete)
            {
                m_previousPlonk = m_currentPlonk;
                m_currentPlonk = nextPlonk;
            
                // Cause the previous Plonk to stop listening locally, so the new
                // Plonk can handle new connection. But we leave the old one running
                // so that it can fulfill outstanding requests.
                m_previousPlonk->StopPortFoward();
            }
        }

        if (!connectSuccess || !connectionComplete)
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

    if (m_meekClientStopSignal)
    {
        m_meekClientStopSignal->SignalStop(STOP_REASON_CANCEL);
    }

    if (m_meekClient != NULL) 
    {
        delete m_meekClient;
        m_meekClient = NULL;
    }

    if (m_meekClientStopInfo != NULL)
    {
        delete m_meekClientStopInfo;
        m_meekClientStopInfo = NULL;
    }

    if (m_meekClientStopSignal != NULL)
    {
        delete m_meekClientStopSignal;
        m_meekClientStopSignal = NULL;
    }

    return true;
}

void SSHTransportBase::TransportConnect()
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

void SSHTransportBase::TransportConnectHelper()
{
    assert(m_systemProxySettings != NULL);

    // Get the ServerEntries we'll try to connect to

    ServerEntries serverEntries;
    if (m_tempConnectServerEntry)
    {
        serverEntries.push_back(*m_tempConnectServerEntry);
    }
    else
    {
        serverEntries = m_serverList.GetList();
        
        // Remove all servers that don't support this transport
        for (int i = (signed)serverEntries.size()-1; i >= 0; --i)
        {
            if (!ServerHasCapabilities(serverEntries[i]))
            {
                serverEntries.erase(serverEntries.begin()+i);
            }
        }

        // Leave the first MULTI_CONNECT_POOL_SIZE servers in place and shuffle the rest
        if (serverEntries.size() > MULTI_CONNECT_POOL_SIZE)
        {
            random_shuffle(serverEntries.begin() + MULTI_CONNECT_POOL_SIZE, serverEntries.end());
        }
    }
        
    if (serverEntries.size() == 0)
    {
        my_print(NOT_SENSITIVE, false, _T("No known servers support this transport type."));

        // Cause this transport's connect sequence (and immediate failover) to 
        // stop. Otherwise we'll quickly fail over and over.
        m_stopInfo.stopSignal->SignalStop(STOP_REASON_CANCEL);
        throw Abort();
    }

    //
    // Start meek
    //

    // Meek will have its own StopInfo. This is because it gets torn down by 
    // SSHTransportBase and should not respond directly to the GlobalStopSignal.
    m_meekClientStopSignal = new StopSignal();
    if (m_meekClientStopSignal == NULL)
    {
        stringstream error;
        error << __FUNCTION__ << ":" << __LINE__ << ": Out of memory";
        throw std::exception(error.str().c_str());
    }

    m_meekClientStopInfo = new StopInfo(m_meekClientStopSignal, STOP_REASON_ALL);
    if (m_meekClientStopInfo == NULL)
    {
        stringstream error;
        error << __FUNCTION__ << ":" << __LINE__ << ": Out of memory";
        throw std::exception(error.str().c_str());
    }

    m_meekClient = new Meek();
    if (m_meekClient == NULL)
    {
        stringstream error;
        error << __FUNCTION__ << ":" << __LINE__ << ": Out of memory";
        throw std::exception(error.str().c_str());
    }

    if (!m_meekClient->Start(*m_meekClientStopInfo, NULL))
    {
        my_print(NOT_SENSITIVE, false, _T("Unable to start meek client executable."));
        m_stopInfo.stopSignal->SignalStop(STOP_REASON_CANCEL);
        throw Abort();
    }

    if(!m_meekClient->WaitForCmethodLine())
	{
        my_print(NOT_SENSITIVE, false, _T("Unable to get meek listening port."));
        m_stopInfo.stopSignal->SignalStop(STOP_REASON_CANCEL);
        throw Abort();
	}
    
    // Extract plonk executable and put to disk if not already

    if (m_plonkPath.size() == 0)
    {
        if (!ExtractExecutable(IDR_PLONK_EXE, PLONK_EXE_NAME, m_plonkPath))
        {
            my_print(NOT_SENSITIVE, false, _T("Unable to extract SSH transport executable."));

            // Cause this transport's connect sequence (and immediate failover) to 
            // stop. Otherwise we'll quickly fail over and over.
            m_stopInfo.stopSignal->SignalStop(STOP_REASON_CANCEL);
            throw Abort();
        }
    }

    // Test if the localSocksProxyPort is already in use.  If it is, try to find
    // one that is available.
    m_localSocksProxyPort = DEFAULT_PLONK_SOCKS_PROXY_PORT;
    if (!TestForOpenPort(m_localSocksProxyPort, 10, m_stopInfo))
    {
        if (!m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
        {
            my_print(NOT_SENSITIVE, false, _T("Local SOCKS proxy could not find an available port."));

            // Cause this transport's connect sequence (and immediate failover) to 
            // stop. Otherwise we'll quickly fail over and over.
            m_stopInfo.stopSignal->SignalStop(STOP_REASON_CANCEL);
        }

        throw Abort();
    }

    size_t totalInitialServers = serverEntries.size();
    my_print(NOT_SENSITIVE, true, _T("%s:%d: Attempting to connect to %d servers, %d at a time."), __TFUNCTION__, __LINE__, totalInitialServers, MULTI_CONNECT_POOL_SIZE);

    /*
    We will be trying to make multiple SSH connections to different servers at 
    the same time. They will all be listening as SOCKS proxies on the same local
    port -- this will work due to the SO_REUSEADDR flag.
    We will emulate a connection pool -- new connections will be created as old
    connections fails, thereby maintaining a constant connection count.
    We will go through all available servers before failing completely.
    */

    struct ConnectionInfo {
        boost::shared_ptr<PlonkConnection> plonkConnection;
        SessionInfo sessionInfo;
        DWORD startTime;
        bool firstServer;  // track this to help with server affinity stuff
    };

    vector<ConnectionInfo> connectionAttempts;
    ConnectionInfo tentativeConnection;
    
    assert(m_currentPlonk.get() == NULL);

    ServerEntryIterator nextServerEntry = serverEntries.begin();

    bool abortServerAffinity = false;
    DWORD start = GetTickCount();

    DWORD lastProgressTime = GetTickCount();
    const DWORD PROGRESS_INTERVAL_MS = 10000;

    while (m_currentPlonk.get() == NULL 
           && (connectionAttempts.size() > 0                // either ongoing connection attempts
               || nextServerEntry != serverEntries.end()))  // or more servers left to try
    {
        if (!tentativeConnection.plonkConnection.get()
            && GetTickCountDiff(lastProgressTime, GetTickCount()) > PROGRESS_INTERVAL_MS)
        {
            my_print(NOT_SENSITIVE, false, _T("Have attempted to connect to %d of %d known servers. Still trying..."), totalInitialServers - (serverEntries.end() - nextServerEntry) - connectionAttempts.size(), totalInitialServers), 
            lastProgressTime = GetTickCount();
        }

        // Iterate in reverse, so we can remove dead connections.
        // On the first iteration of the outer loop, connectionAttempts is
        // empty, so we skip this loop.
        for (int i = (signed)connectionAttempts.size()-1; i >= 0; --i)
        {
            if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
            {
                throw Abort();
            }

            bool connected = false;
            if (!connectionAttempts[i].plonkConnection->CheckForConnected(connected))
            {
                // The connection failed

                // If this is the first server failing, then we should
                // stop trying to promote server affinity.
                if (connectionAttempts[i].firstServer)
                {
                    abortServerAffinity = true;
                }

                my_print(NOT_SENSITIVE, true, _T("%s:%d: Server connect FAILED, removing: %S. Servers connecting: %d. Servers remaining: %d."), __TFUNCTION__, __LINE__, connectionAttempts[i].sessionInfo.GetServerAddress().c_str(), connectionAttempts.size(), serverEntries.end() - nextServerEntry);

                connectionAttempts.erase(connectionAttempts.begin()+i);

                // Don't mark the fast-failed server as failed (i.e., don't 
                // move it to the back of the server list). Fast failures don't
                // necessarily indicate a bad server -- e.g., it can happen if the
                // server is temporarily overloaded.
            }
            else if (connected)
            {
                // Because we're trying to promote server affinity, what we do 
                // here depends on whether this is the first server, and, if 
                // not, whether the first server's "head start" has expired.

                // Don't overwrite the tentative connection if it's set to the first server.
                if (!tentativeConnection.plonkConnection.get() || !tentativeConnection.firstServer)
                {
                    tentativeConnection = connectionAttempts[i];
                }

                // Mark the server as succeeded.
                MarkServerSucceeded(tentativeConnection.sessionInfo.GetServerEntry());

                my_print(NOT_SENSITIVE, true, _T("%s:%d: Server connect SUCCEEDED, removing: %S. Servers connecting: %d. Servers remaining: %d."), __TFUNCTION__, __LINE__, connectionAttempts[i].sessionInfo.GetServerAddress().c_str(), connectionAttempts.size(), serverEntries.end() - nextServerEntry);
                
                connectionAttempts.erase(connectionAttempts.begin()+i);

                // Otherwise keep checking for successful connections.
            }
            else if (GetTickCountDiff(connectionAttempts[i].startTime, GetTickCount()) > SSH_CONNECTION_TIMEOUT_SECONDS*1000)
            {
                // Connection attempt time expired

                // If this is the first server failing, then we should
                // stop trying to promote server affinity.
                if (connectionAttempts[i].firstServer)
                {
                    abortServerAffinity = true;
                }

                my_print(NOT_SENSITIVE, true, _T("%s:%d: Server connect TIMED OUT, removing: %S. Servers connecting: %d. Servers remaining: %d."), __TFUNCTION__, __LINE__, connectionAttempts[i].sessionInfo.GetServerAddress().c_str(), connectionAttempts.size(), serverEntries.end() - nextServerEntry);

                MarkServerFailed(connectionAttempts[i].sessionInfo.GetServerEntry());

                connectionAttempts.erase(connectionAttempts.begin()+i);

                // If our connection sequence is taking so long that we're 
                // exceeding the SSH connection timeout, we should trigger a
                // remote server list fetch.
                if (m_remoteServerListFetcher)
                {
                    m_remoteServerListFetcher->FetchRemoteServerList();
                }
            }
        }

        // If we have a connected server, determine if we're ready to go or
        // still giving the first server a chance to connect.
        if (tentativeConnection.plonkConnection.get())
        {
            if (tentativeConnection.firstServer
                || abortServerAffinity
                || GetTickCountDiff(start, GetTickCount()) > SERVER_AFFINITY_HEAD_START_MS)
            {
                break;
            }
            else // give the first server more of a chance
            {
                my_print(NOT_SENSITIVE, true, _T("%s:%d: Got connection, but waiting for first server head start."), __TFUNCTION__, __LINE__);
            }
        }

        // If the connection pool isn't maxed out, fill it up.
        while (connectionAttempts.size() < MULTI_CONNECT_POOL_SIZE
               && nextServerEntry != serverEntries.end())
        {
            if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
            {
                throw Abort();
            }

            ConnectionInfo newConnection;
            newConnection.firstServer = (nextServerEntry == serverEntries.begin());
            newConnection.startTime = GetTickCount();
            newConnection.sessionInfo.Set(*nextServerEntry++);

            if (InitiateConnection(m_meekClient->GetListenPort(), 
                    newConnection.firstServer, 
                    newConnection.sessionInfo, 
                    newConnection.plonkConnection))
            {
                tstring transportRequestName, serverAddress;
                newConnection.plonkConnection->GetTransportInfo(transportRequestName, serverAddress);
                tstring serverEntryAddress = NarrowToTString(newConnection.sessionInfo.GetServerAddress());
                if (serverAddress != serverEntryAddress)
                {
                    serverAddress = serverEntryAddress + _T(" via ") + serverAddress;
                }

                my_print(
                    NOT_SENSITIVE, true, 
                    _T("%s:%d: Server connect STARTED, adding: %s, using %s. Servers connecting: %d. Servers remaining: %d."), 
                    __TFUNCTION__, __LINE__, 
                    serverAddress.c_str(), 
                    transportRequestName.c_str(), 
                    connectionAttempts.size(), 
                    serverEntries.end() - nextServerEntry);

                connectionAttempts.push_back(newConnection);
            }
        }

        Sleep(100);
    }

    if (tentativeConnection.plonkConnection.get() != NULL)
    {
        // Make sure that the server we're using is at the front of the list
        MarkServerSucceeded(tentativeConnection.sessionInfo.GetServerEntry());

        m_currentPlonk = tentativeConnection.plonkConnection;
        m_sessionInfo = tentativeConnection.sessionInfo;
    }
    else
    {
        throw TransportFailed();
    }

    assert(m_currentPlonk.get() != NULL);

    // We got our connected Plonk connection. We'll let all other PlonkConnections 
    // go out of scope, thereby getting cleaned up.

    m_systemProxySettings->SetSocksProxyPort(m_localSocksProxyPort);

    // Record which server we're using
    tstring transportRequestName, serverAddress;
    m_currentPlonk->GetTransportInfo(transportRequestName, serverAddress);

    ostringstream ss;
    ss << "ipAddress: " << m_sessionInfo.GetServerAddress() << "\n";
    ss << "connType: " << TStringToNarrow(transportRequestName) << "\n";
    if (serverAddress != NarrowToTString(m_sessionInfo.GetServerAddress())) 
    {
        ss << "front: " << TStringToNarrow(serverAddress) << "\n";
    }
    AddDiagnosticInfoYaml("ConnectedServer", ss.str().c_str());

    my_print(NOT_SENSITIVE, false, _T("SOCKS proxy is running on localhost port %d."), m_localSocksProxyPort);
}


bool SSHTransportBase::GetUserParentProxySettings(
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
    return(systemProxySettings->GetUserLanProxy(
        o_UserSSHParentProxyType, 
        o_UserSSHParentProxyHostname, 
        o_UserSSHParentProxyPort));
}


bool SSHTransportBase::InitiateConnection(
    int meekListenPort,
    bool firstServer,
    const SessionInfo& sessionInfo,
    boost::shared_ptr<PlonkConnection>& o_plonkConnection)
{
    o_plonkConnection.reset();

    // Start plonk using Psiphon server SSH parameters

    tstring serverAddress, serverHostKey, transportRequestName, plonkCommandLine;
    int serverPort;

    GetSSHParams(
        meekListenPort,
        firstServer,
        sessionInfo,
        m_localSocksProxyPort, 
        m_systemProxySettings,
        serverAddress, 
        serverPort, 
        serverHostKey, 
        transportRequestName,
        plonkCommandLine);

    boost::shared_ptr<PlonkConnection> plonkConnection(new PlonkConnection(sessionInfo));

    if (plonkConnection.get() == NULL)
    {
        stringstream error;
        error << __FUNCTION__ << ":" << __LINE__ << ": Out of memory";
        throw std::exception(error.str().c_str());
    }

    // Record the connection attempt
    ostringstream ss;
    ss << "ipAddress: " << sessionInfo.GetServerAddress() << "\n";
    ss << "connType: " << TStringToNarrow(transportRequestName) << "\n";
    if (serverAddress != NarrowToTString(sessionInfo.GetServerAddress())) 
    {
        ss << "front: " << TStringToNarrow(serverAddress) << "\n";
    }

    AddDiagnosticInfoYaml("ConnectingServer", ss.str().c_str());

    bool success = plonkConnection->Connect(
                                    m_localSocksProxyPort,
                                    serverAddress.c_str(),
                                    serverHostKey.c_str(),
                                    m_plonkPath.c_str(),
                                    plonkCommandLine.c_str(),
                                    serverPort,
                                    transportRequestName.c_str(),
                                    m_stopInfo);

    if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
    {
        throw Abort();
    }

    if (!success)
    {
        return false;
    }

    o_plonkConnection = plonkConnection;

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
      m_plonkOutputHandle(INVALID_HANDLE_VALUE),
      m_connected(false),
      m_localSocksProxyPort(-1),
      m_serverPort(-1)
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
        my_print(NOT_SENSITIVE, true, _T("%s:%d:%s: Fresh limit: %u"), __TFUNCTION__, __LINE__, m_serverAddress.c_str(), freshLimit);
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
        my_print(NOT_SENSITIVE, true, _T("%s:%d:%s: Retired limit: %u"), __TFUNCTION__, __LINE__, m_serverAddress.c_str(), retiredLimit);
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
    m_connected = false;
    m_localSocksProxyPort = -1;
    m_serverAddress.clear(); 
    m_serverHostKey.clear(); 
    m_plonkPath.clear();
    m_plonkCommandLine.clear();
    m_serverPort = -1;

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
        LPCTSTR transportRequestName,
        const StopInfo& stopInfo)
{
    // Ensure we start from a disconnected/clean state
    Kill();

    m_localSocksProxyPort = localSocksProxyPort;
    m_serverAddress = serverAddress; 
    m_serverHostKey = serverHostKey; 
    m_plonkPath = plonkPath;
    m_plonkCommandLine = plonkCommandLine;
    m_serverPort = serverPort;
    m_transportRequestName = transportRequestName;
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
        my_print(NOT_SENSITIVE, false, _T("%s:%d:%s: CreatePolipoPipe failed (%d)"), __TFUNCTION__, __LINE__, m_serverAddress.c_str(), GetLastError());
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
        my_print(NOT_SENSITIVE, false, _T("%s:%d:%s: CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, m_serverAddress.c_str(), GetLastError());
        return false;
    }
    if (!CloseHandle(plonkStartupInfo.hStdError))
    {
        CloseHandle(plonkOutput);
        CloseHandle(plonkInput);
        CloseHandle(plonkStartupInfo.hStdInput);
        my_print(NOT_SENSITIVE, false, _T("%s:%d:%s: CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, m_serverAddress.c_str(), GetLastError());
        return false;
    }
    if (!CloseHandle(plonkStartupInfo.hStdInput))
    {
        CloseHandle(plonkOutput);
        CloseHandle(plonkInput);
        my_print(NOT_SENSITIVE, false, _T("%s:%d:%s: CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, m_serverAddress.c_str(), GetLastError());
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
// Returns true if no error occurred. Returns false if Plonk has died/failed.
// Connected status is indicated by `o_connected`.
bool PlonkConnection::CheckForConnected(bool& o_connected)
{
    o_connected = false;

    // Once we're connected, we can't keep checking the pipe, so we'll use a
    // cached success value.
    if (m_connected)
    {
        o_connected = true;
        return true;
    }

    assert(m_stopInfo != NULL);
    assert(m_plonkOutputHandle != INVALID_HANDLE_VALUE);

    SetLastError(ERROR_SUCCESS);

    DWORD bytes_avail = 0;

    // ReadFile will block forever if there's no data to read, so we need
    // to check if there's data available to read first.
    if (!PeekNamedPipe(m_plonkOutputHandle, NULL, 0, NULL, &bytes_avail, NULL))
    {
        my_print(NOT_SENSITIVE, true, _T("%s:%d:%s: PeekNamedPipe failed (%d)"), __TFUNCTION__, __LINE__, m_serverAddress.c_str(), GetLastError());
        return false;
    }

    // If there's data available from the Plonk pipe, process it.
    if (bytes_avail > 0)
    {
        char* buffer = new char[bytes_avail+1];
        DWORD num_read = 0;
        if (!ReadFile(m_plonkOutputHandle, buffer, bytes_avail, &num_read, NULL))
        {
            my_print(NOT_SENSITIVE, false, _T("%s:%d:%s: ReadFile failed (%d)"), __TFUNCTION__, __LINE__, m_serverAddress.c_str(), GetLastError());
            false;
        }
        buffer[bytes_avail] = '\0';

        // Note that we are only capturing Plonk output during the connect sequence.
        my_print(NOT_SENSITIVE, true, _T("%s:%d:%s: Plonk output: >>>%S<<<"), __TFUNCTION__, __LINE__, m_serverAddress.c_str(), buffer);

        bool connected = (strstr(buffer, "PSIPHON:CONNECTED") != NULL);

        delete[] buffer;

        if (connected)
        {
            // We're done reading Plonk output
            if (!CloseHandle(m_plonkOutputHandle))
            {
                my_print(NOT_SENSITIVE, false, _T("%s:%d:%s: CloseHandle failed (%d)"), __TFUNCTION__, __LINE__, m_serverAddress.c_str(), GetLastError());
                return false;
            }
            m_plonkOutputHandle = INVALID_HANDLE_VALUE;

            // We got the expected output from Plonk and we're ready to go.
            o_connected = true;
        }
    }

    m_connected = o_connected;
    return true;
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


void PlonkConnection::GetConnectParams(
        int& o_localSocksProxyPort,
        tstring& o_serverAddress, 
        tstring& o_serverHostKey, 
        tstring& o_plonkPath, 
        tstring& o_plonkCommandLine,
        int& o_serverPort,
        tstring& o_transportRequestName) const
{
    o_localSocksProxyPort = m_localSocksProxyPort;
    o_serverAddress = m_serverAddress;
    o_serverHostKey = m_serverHostKey;
    o_plonkPath = m_plonkPath;
    o_plonkCommandLine = m_plonkCommandLine;
    o_serverPort = m_serverPort;
    o_transportRequestName = m_transportRequestName;
}

void PlonkConnection::GetTransportInfo(tstring& o_transportRequestName, tstring& o_serverAddress) const
{
    o_transportRequestName = m_transportRequestName;
    o_serverAddress = m_serverAddress;
}

void PlonkConnection::UpdateSessionInfo(const SessionInfo& sessionInfo)
{
    m_sessionInfo = sessionInfo;
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
    o_addServerEntriesFn = ITransport::AddServerEntries;
}


SSHTransport::SSHTransport()
    : SSHTransportBase(GetTransportProtocolName().c_str())
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

bool SSHTransport::ServerHasCapabilities(const ServerEntry& entry) const
{
    return entry.HasCapability(TStringToNarrow(GetTransportProtocolName()));
}

void SSHTransport::GetSSHParams(
    int meekListenPort,
    bool firstServer,
    const SessionInfo& sessionInfo,
    const int localSocksProxyPort,
    SystemProxySettings* systemProxySettings,
    tstring& o_serverAddress, 
    int& o_serverPort, 
    tstring& o_serverHostKey, 
    tstring& o_transportRequestName,
    tstring& o_plonkCommandLine)
{
    o_serverAddress.clear();
    o_serverPort = 0;
    o_serverHostKey.clear();
    o_transportRequestName.clear();
    o_plonkCommandLine.clear();

    o_serverAddress = NarrowToTString(sessionInfo.GetServerAddress());
    o_serverPort = sessionInfo.GetSSHPort();
    o_serverHostKey = NarrowToTString(sessionInfo.GetSSHHostKey());
    o_transportRequestName = SSH_REQUEST_PROTOCOL_NAME;

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
        firstServer,
        sessionInfo,
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
    o_plonkCommandLine += _T(" ") + o_serverAddress;
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
    o_addServerEntriesFn = ITransport::AddServerEntries;
}


OSSHTransport::OSSHTransport()
    : SSHTransportBase(OSSH_TRANSPORT_PROTOCOL_NAME)
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
    //this will be changed when a transport connection is established, 
    //depending if it's fronted meek, unfronted meek, or bare OSSH
    tstring transportRequestName, serverAddress;
    m_currentPlonk->GetTransportInfo(transportRequestName, serverAddress);
    return transportRequestName;
}

bool OSSHTransport::ServerHasCapabilities(const ServerEntry& entry) const
{
    return ( entry.HasCapability(TStringToNarrow(GetTransportProtocolName())) ||
        entry.HasCapability(string(CAPABILITY_UNFRONTED_MEEK)) ||
         entry.HasCapability(string(CAPABILITY_FRONTED_MEEK)));
}

void OSSHTransport::GetSSHParams(
    int meekListenPort,
    bool firstServer,
    const SessionInfo& sessionInfo,
    const int localSocksProxyPort,
    SystemProxySettings* systemProxySettings,
    tstring& o_serverAddress, 
    int& o_serverPort, 
    tstring& o_serverHostKey, 
    tstring& o_transportRequestName,
    tstring& o_plonkCommandLine)
{
    tstring proxy_type, proxy_host, proxy_username, proxy_password;
    int proxy_port;

    o_serverAddress.clear();
    o_serverPort = 0;
    o_serverHostKey.clear();
    o_transportRequestName.clear();
    o_plonkCommandLine.clear();

    o_serverAddress = NarrowToTString(sessionInfo.GetServerAddress());
    o_serverPort = sessionInfo.GetSSHObfuscatedPort();
    o_serverHostKey = NarrowToTString(sessionInfo.GetSSHHostKey());
    o_transportRequestName = OSSH_REQUEST_PROTOCOL_NAME;

    // Client transmits its session ID prepended to the SSH password; the server
    // uses this to associate the tunnel with web requests -- for GeoIP region stats
    string sshPassword = sessionInfo.GetClientSessionID() + sessionInfo.GetSSHPassword();

    // Note: -batch ensures plonk doesn't hang on a prompt when the server's host key isn't
    // the expected value we just set in the registry

    tstringstream args;
    args << _T(" -ssh -C -N -batch");

    if(GetUserParentProxySettings(
        firstServer,
        sessionInfo,
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
    else if (sessionInfo.GetServerEntry().HasCapability(CAPABILITY_FRONTED_MEEK) || 
        sessionInfo.GetServerEntry().HasCapability(CAPABILITY_UNFRONTED_MEEK))
    {
        if(sessionInfo.GetServerEntry().HasCapability(CAPABILITY_FRONTED_MEEK))
        {
            o_serverAddress = NarrowToTString(sessionInfo.GetMeekFrontingDomain());
            o_serverPort = 443; 
            o_transportRequestName = FRONTED_MEEK_REQUEST_PROTOCOL_NAME;
        }
        else if(sessionInfo.GetServerEntry().HasCapability(CAPABILITY_UNFRONTED_MEEK)) 
        {
            o_serverAddress = NarrowToTString(sessionInfo.GetServerAddress());
            o_serverPort = sessionInfo.GetMeekServerPort();
            o_transportRequestName = UNFRONTED_MEEK_REQUEST_PROTOCOL_NAME;
        }

        //Parameters passed to the meek client via SOCKS interface(via -proxy_username)
        
        string sArg;

        args << _T(" -proxy_username ");

        sArg = sessionInfo.GetServerAddress();
        args << _T("pserver=") << EscapeSOCKSArg(sArg.c_str()) << _T(":") <<  sessionInfo.GetSSHObfuscatedPort() << _T(";");

        sArg = sessionInfo.GetClientSessionID();
        args << _T("sshid=") << EscapeSOCKSArg(sArg.c_str()) << _T(";");

        sArg = sessionInfo.GetMeekObfuscatedKey();
        args << _T("obfskey=") << EscapeSOCKSArg(sArg.c_str()) << _T(";");


        sArg = sessionInfo.GetMeekFrontingHost();
        if(!sArg.empty())
        {
            args << _T("fhostname=") << EscapeSOCKSArg(sArg.c_str()) << _T(";");
        }
        sArg = sessionInfo.GetMeekCookieEncryptionPublicKey();
        args << _T("cpubkey=") << EscapeSOCKSArg(sArg.c_str());

        args << _T(" -proxy_type socks4a");
        args << _T(" -proxy_host 127.0.0.1");
        args << _T(" -proxy_port ") << meekListenPort;
    }

    if (o_serverPort <= 0 
        || o_serverAddress.empty()
        || sessionInfo.GetSSHObfuscatedKey().empty())
    {
        my_print(NOT_SENSITIVE, false, _T("%s - missing parameters"), __TFUNCTION__);

        // TODO: Is this actually a fatal error? Throw std::exception?
        throw TransportFailed();
    }

    args << _T(" -P ") << o_serverPort
        << _T(" -l ") << NarrowToTString(sessionInfo.GetSSHUsername()).c_str()
        << _T(" -pw ") << NarrowToTString(sshPassword).c_str()
        << _T(" -D ") << localSocksProxyPort;

    // Now using this flag for debug and release. We use the verbose Plonk 
    // output to determine when it has successfully connected.
    args << _T(" -v");
    args << _T(" -z -Z ") << NarrowToTString(sessionInfo.GetSSHObfuscatedKey());
    
    o_plonkCommandLine = m_plonkPath + args.str();
    o_plonkCommandLine += _T(" ") + o_serverAddress;
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

