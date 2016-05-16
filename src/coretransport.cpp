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
#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")
#include "shlobj.h"

#include "logging.h"
#include "coretransport.h"
#include "sessioninfo.h"
#include "psiclient.h"
#include "utilities.h"
#include "systemproxysettings.h"
#include "config.h"
#include "diagnostic_info.h"
#include "embeddedvalues.h"
#include "utilities.h"

using namespace std::experimental;


#define AUTOMATICALLY_ASSIGNED_PORT_NUMBER   0
#define EXE_NAME                             _T("psiphon-tunnel-core.exe")
#define URL_PROXY_EXE_NAME                   _T("psiphon-url-proxy.exe")
#define MAX_LEGACY_SERVER_ENTRIES            30
#define LEGACY_SERVER_ENTRY_LIST_NAME        (string(LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS) + "OSSH").c_str()


/******************************************************************************
 CoreTransport
******************************************************************************/

CoreTransport::CoreTransport()
    : ITransport(CORE_TRANSPORT_PROTOCOL_NAME),
      m_pipe(NULL),
      m_localSocksProxyPort(AUTOMATICALLY_ASSIGNED_PORT_NUMBER),
      m_localHttpProxyPort(AUTOMATICALLY_ASSIGNED_PORT_NUMBER),
      m_hasEverConnected(false),
      m_isConnected(false)
{
    ZeroMemory(&m_processInfo, sizeof(m_processInfo));
}


CoreTransport::~CoreTransport()
{
    (void)Cleanup();
    IWorkerThread::Stop();
}


bool CoreTransport::Cleanup()
{
     // Give the process an opportunity for graceful shutdown, then terminate
    if (m_processInfo.hProcess != 0
        && m_processInfo.hProcess != INVALID_HANDLE_VALUE)
    {
        bool stoppedGracefully = false;

        // Allows up to 2 seconds for core process to gracefully shutdown.
        // This gives it an opportunity to send final status requests, and
        // to persist tunnel stats that cannot yet be reported.
        // While waiting, continue to consume core process output.
        // TODO: AttachConsole/FreeConsole sequence not threadsafe?
        if (AttachConsole(m_processInfo.dwProcessId))
        {
            GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_processInfo.dwProcessId);
            FreeConsole();

            for (int i = 0; i < 2000; i += 10)
            {
                ConsumeCoreProcessOutput();
                if (WAIT_OBJECT_0 == WaitForSingleObject(m_processInfo.hProcess, 10))
                {
                    stoppedGracefully = true;
                    break;
                }
            }
        }
        if (!stoppedGracefully)
        {
            if (!TerminateProcess(m_processInfo.hProcess, 0) ||
                WAIT_OBJECT_0 != WaitForSingleObject(m_processInfo.hProcess, TERMINATE_PROCESS_WAIT_MS))
            {
                my_print(NOT_SENSITIVE, false, _T("TerminateProcess failed for process with PID %d"), m_processInfo.dwProcessId);
            }
        }
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
    m_pipeBuffer.clear();

    m_hasEverConnected = false;
    m_isConnected = false;

    return true;
}


// Support the registration of this transport type
static ITransport* NewCoreTransport()
{
    return new CoreTransport();
}


// static
void CoreTransport::GetFactory(
                    tstring& o_transportDisplayName,
                    tstring& o_transportProtocolName,
                    TransportFactoryFn& o_transportFactoryFn,
                    AddServerEntriesFn& o_addServerEntriesFn)
{
    o_transportFactoryFn = NewCoreTransport;
    o_transportDisplayName = CORE_TRANSPORT_DISPLAY_NAME;
    o_transportProtocolName = CORE_TRANSPORT_PROTOCOL_NAME;
    // Note: these server entries are not used as the core manages its own
    // database of entries.
    // TODO: add code to harvest these entries?
    o_addServerEntriesFn = ITransport::AddServerEntries;
}


tstring CoreTransport::GetTransportProtocolName() const 
{
    return CORE_TRANSPORT_PROTOCOL_NAME;
}


tstring CoreTransport::GetTransportDisplayName() const 
{ 
    return CORE_TRANSPORT_DISPLAY_NAME; 
}


tstring CoreTransport::GetTransportRequestName() const
{
    return GetTransportProtocolName();
}


bool CoreTransport::IsHandshakeRequired() const
{
    return false;
}


bool CoreTransport::IsWholeSystemTunneled() const
{
    return false;
}


bool CoreTransport::ServerWithCapabilitiesExists()
{
    // For now, we assume there are sufficient server entries for SSH/OSSH.
    // Even if there are not any in the core database, it will automatically
    // perform its own remote server list fetch.
    return true;
}


bool CoreTransport::ServerHasCapabilities(const ServerEntry& entry) const
{
    // At the moment, this is only called by ServerRequest::GetTempTransports
    // to check if the core can establish a temporary tunnel to the given
    // server. Since the core's supported protocols and future server entry
    // capabilities are opaque and unknown, respectively, we will just report
    // that the core supports this server and let it fail to establish if
    // it does not.
    return true;
}


bool CoreTransport::RequiresStatsSupport() const
{
    return false;
}


tstring CoreTransport::GetSessionID(const SessionInfo& sessionInfo)
{
    return UTF8ToWString(sessionInfo.GetSSHSessionID());
}


int CoreTransport::GetLocalProxyParentPort() const
{
    // Should not be called for core.
    assert(false);
    return 0;
}


tstring CoreTransport::GetLastTransportError() const
{
    return _T("0");
}


void CoreTransport::TransportConnect()
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


bool CoreTransport::RequestingUrlProxyWithoutTunnel()
{
    // If the transport is invoked with a temp tunnel
    // server entry having a blank address, this is a special
    // case where the core is being used for direct connections
    // through its url proxy. No tunnel will be established.
    return m_tempConnectServerEntry != 0 &&
        m_tempConnectServerEntry->serverAddress.empty();
}

    
void CoreTransport::TransportConnectHelper()
{
    assert(m_systemProxySettings != NULL);

    tstring configFilename, serverListFilename;
    if (!WriteParameterFiles(configFilename, serverListFilename))
    {
        throw TransportFailed();
    }

    // Run core process; it will begin establishing a tunnel

    if (!SpawnCoreProcess(configFilename, serverListFilename))
    {
        throw TransportFailed();
    }

    // Wait and poll for first active tunnel (or stop signal)

    while (true)
    {
        // Check that the process is still running and consume output
        if (!DoPeriodicCheck())
        {
            throw TransportFailed();
        }

        if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
        {
            throw Abort();
        }

        if (m_isConnected)
        {
            break;
        }

        Sleep(100);
    }

    m_systemProxySettings->SetSocksProxyPort(m_localSocksProxyPort);
    my_print(NOT_SENSITIVE, false, _T("SOCKS proxy is running on localhost port %d."), m_localSocksProxyPort);

    m_systemProxySettings->SetHttpProxyPort(m_localHttpProxyPort);
    m_systemProxySettings->SetHttpsProxyPort(m_localHttpProxyPort);
    my_print(NOT_SENSITIVE, false, _T("HTTP proxy is running on localhost port %d."), m_localHttpProxyPort);
}


bool CoreTransport::WriteParameterFiles(tstring& configFilename, tstring& serverListFilename)
{
    TCHAR path[MAX_PATH];
    if (!SHGetSpecialFolderPath(NULL, path, CSIDL_APPDATA, FALSE))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - SHGetFolderPath failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }
    filesystem::path appDataPath(path);

    auto dataStoreDirectory = filesystem::path(appDataPath).append(LOCAL_SETTINGS_APPDATA_SUBDIRECTORY);
    if (!CreateDirectory(dataStoreDirectory.c_str(), NULL) && ERROR_ALREADY_EXISTS != GetLastError())
    {
        my_print(NOT_SENSITIVE, false, _T("%s - create directory failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    // Passing short path names for data store directories due to sqlite3 incompatibility
    // with extended Unicode characters in paths (e.g., unicode user name in AppData or
    // Temp path)
    tstring shortDataStoreDirectory;
    if (!GetShortPathName(dataStoreDirectory, shortDataStoreDirectory))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - GetShortPathName failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    Json::Value config;
    config["ClientPlatform"] = CLIENT_PLATFORM;
    config["ClientVersion"] = CLIENT_VERSION;
    config["PropagationChannelId"] = PROPAGATION_CHANNEL_ID;
    config["SponsorId"] = SPONSOR_ID;
    config["RemoteServerListUrl"] = string("https://") + REMOTE_SERVER_LIST_ADDRESS + "/" + REMOTE_SERVER_LIST_REQUEST_PATH;
    config["RemoteServerListSignaturePublicKey"] = REMOTE_SERVER_LIST_SIGNATURE_PUBLIC_KEY;
    config["DataStoreDirectory"] = WStringToUTF8(shortDataStoreDirectory);
    config["UseIndistinguishableTLS"] = true;
    config["DeviceRegion"] = WStringToUTF8(GetDeviceRegion());
    config["EmitDiagnosticNotices"] = true;

    // Don't use an upstream proxy when in VPN mode. If the proxy is on a private network,
    // we may not be able to route to it. If the proxy is on a public network we prefer not
    // to use it for Psiphon requests (this assumes that this core transport has been created
    // as a temp tunnel or url proxy facilitator, since some underlying transport is already
    // providing whole system tunneling).
    if (!g_connectionManager.IsWholeSystemTunneled())
    {
        string proxyAddress = GetUpstreamProxyAddress();
        if (proxyAddress.length() > 0)
        {
            config["UpstreamProxyUrl"] = "http://"+proxyAddress;
        }
    }

    if (Settings::SplitTunnel())
    {
        config["SplitTunnelRoutesUrlFormat"] = SPLIT_TUNNEL_ROUTES_URL_FORMAT;
        config["SplitTunnelRoutesSignaturePublicKey"] = SPLIT_TUNNEL_ROUTES_SIGNATURE_PUBLIC_KEY;
        config["SplitTunnelDnsServer"] = SPLIT_TUNNEL_DNS_SERVER;
    }

    // In temporary tunnel mode, only the specific server should be connected to,
    // and a handshake is not performed.
    // For example, in VPN mode, the temporary tunnel is used by the VPN mode to
    // perform its own handshake request to get a PSK.
    //
    // This same minimal setup is used in the RequireUrlProxyWithoutTunnel mode,
    // although in this case we don't set a deadline to connect since we don't
    // expect to ever connect to a tunnel and we we want to allow the caller to
    // complete the (direct) url proxied request
    if (m_tempConnectServerEntry != 0 || RequestingUrlProxyWithoutTunnel())
    {
        config["DisableApi"] = true;
        config["DisableRemoteServerListFetcher"] = true;
        string serverEntry = m_tempConnectServerEntry->ToString();
        config["TargetServerEntry"] =
            Hexlify((const unsigned char*)(serverEntry.c_str()), serverEntry.length());
        // Use whichever region the server entry is located in
        config["EgressRegion"] = "";

        if (RequestingUrlProxyWithoutTunnel())
        {
            // The URL proxy can and will be used while the main tunnel is connected, 
            // and multiple URL proxies might be used concurrently. Each one may/will
            // try to open/create the tunnel-core datastore, so conflicts will occur
            // if they try to use the same datastore directory as the main tunnel or
            // as each other. So we'll give each one a unique, temporary directory.

            tstring tempDir;
            if (!GetUniqueTempDir(tempDir, true))
            {
                my_print(NOT_SENSITIVE, false, _T("%s - GetUniqueTempDir failed (%d)"), __TFUNCTION__, GetLastError());
                return false;
            }

            // Passing short path names for data store directories due to sqlite3 incompatibility
            // with extended Unicode characters in paths (e.g., unicode user name in AppData or
            // Temp path).
            tstring shortTempDir;
            if (!GetShortPathName(tempDir, shortTempDir))
            {
                my_print(NOT_SENSITIVE, false, _T("%s - GetShortPathName failed (%d)"), __TFUNCTION__, GetLastError());
                return false;
            }

            config["DataStoreDirectory"] = WStringToUTF8(shortTempDir);
        }
        else
        {
            config["EstablishTunnelTimeoutSeconds"] = TEMPORARY_TUNNEL_TIMEOUT_SECONDS;
        }
    }
    else
    {
        config["EgressRegion"] = Settings::EgressRegion();
        config["LocalHttpProxyPort"] = Settings::LocalHttpProxyPort();
        config["LocalSocksProxyPort"] = Settings::LocalSocksProxyPort();

        auto remoteServerListFilename = filesystem::path(dataStoreDirectory)
                                                    .append(LOCAL_SETTINGS_APPDATA_REMOTE_SERVER_LIST_FILENAME);
        config["RemoteServerListDownloadFilename"] = WStringToUTF8(remoteServerListFilename.wstring());
    }

    ostringstream configDataStream;
    Json::FastWriter jsonWriter;
    configDataStream << jsonWriter.write(config);

    // RequireUrlProxyWithoutTunnel mode has a distinct config file so that
    // it won't conflict with a standard CoreTransport which may already be
    // running. Also, this mode omits the server list file, since it's not
    // trying to establish a tunnel.
    // TODO: there's still a remote chance that concurrently spawned url
    // proxy instances could clobber each other's config file?

    auto configPath = filesystem::path(dataStoreDirectory);
    if (RequestingUrlProxyWithoutTunnel()) 
    {
        configPath.append(LOCAL_SETTINGS_APPDATA_URL_PROXY_CONFIG_FILENAME);
    }
    else
    {
        configPath.append(LOCAL_SETTINGS_APPDATA_CONFIG_FILENAME);
    }
    configFilename = configPath;

    if (!WriteFile(configFilename, configDataStream.str()))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - write config file failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    if (!RequestingUrlProxyWithoutTunnel())
    {
        auto serverListPath = filesystem::path(dataStoreDirectory)
                                          .append(LOCAL_SETTINGS_APPDATA_SERVER_LIST_FILENAME);
        serverListFilename = serverListPath;

        string serverList = EMBEDDED_SERVER_LIST;

        // Retain some existing server entries that were used by the legacy client
        ServerEntries legacyEntries = ServerList::GetListFromSystem(LEGACY_SERVER_ENTRY_LIST_NAME);
        if (legacyEntries.size() > MAX_LEGACY_SERVER_ENTRIES)
        {
            legacyEntries.resize(MAX_LEGACY_SERVER_ENTRIES);
        }
        if (legacyEntries.size() > 0 && serverList.length() > 0)
        {
            // EMBEDDED_SERVER_LIST may be LF-delimited, not LF-terminated
            serverList += "\n";
        }
        serverList += ServerList::EncodeServerEntries(legacyEntries);

        if (!WriteFile(serverListFilename, serverList))
        {
            my_print(NOT_SENSITIVE, false, _T("%s - write server list file failed (%d)"), __TFUNCTION__, GetLastError());
            return false;
        }
    }

    return true;
}

    
string CoreTransport::GetUpstreamProxyAddress()
{
    // Note: upstream SOCKS proxy and proxy auth currently not supported

    if (Settings::SkipUpstreamProxy())
    {
        // Don't use an upstream proxy of any kind.
        return "";
    }

    ostringstream upstreamProxyAddress;

    if (Settings::UpstreamProxyHostname().length() > 0 && 
        Settings::UpstreamProxyPort() &&
        Settings::UpstreamProxyType() == "https")
    {
        // Use a custom, user-set upstream proxy
        upstreamProxyAddress << Settings::UpstreamProxyHostname() << ":" << Settings::UpstreamProxyPort();
    }
    else
    {
        // Use the native default proxy (that is, the one that was set before we tried to connect).
        DecomposedProxyConfig proxyConfig;
        GetNativeDefaultProxyInfo(proxyConfig);

        if (!proxyConfig.httpsProxy.empty())
        {
            upstreamProxyAddress <<
				WStringToUTF8(proxyConfig.httpsProxy) << ":" << proxyConfig.httpsProxyPort;
        }
    }

    return upstreamProxyAddress.str();
}


bool CoreTransport::SpawnCoreProcess(const tstring& configFilename, const tstring& serverListFilename)
{
    tstringstream commandLine;

    if (m_exePath.size() == 0)
    {
        if (RequestingUrlProxyWithoutTunnel())
        {
            // In RequestingUrlProxyWithoutTunnel mode, we allow for multiple instances
            // so we don't fail extract if the file already exists -- and don't try to
            // kill any associated process holding a lock on it.
            if (!ExtractExecutable(
                    IDR_PSIPHON_TUNNEL_CORE_EXE, URL_PROXY_EXE_NAME, m_exePath, true))
            {
                return false;
            }
        }
        else
        {
            if (!ExtractExecutable(
                    IDR_PSIPHON_TUNNEL_CORE_EXE, EXE_NAME, m_exePath))
            {
                return false;
            }
        }
    }

    commandLine << m_exePath
        << _T(" --config \"") << configFilename << _T("\"");

    if (!RequestingUrlProxyWithoutTunnel())
    {
        commandLine << _T(" --serverList \"") << serverListFilename << _T("\"");
    }

    STARTUPINFO startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

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
        my_print(NOT_SENSITIVE, false, _T("%s - CreateSubprocessPipes failed (%d)"), __TFUNCTION__, GetLastError());
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


void CoreTransport::ConsumeCoreProcessOutput()
{
    DWORD bytes_avail = 0;

    // ReadFile will block forever if there's no data to read, so we need
    // to check if there's data available to read first.
    if (!PeekNamedPipe(m_pipe, NULL, 0, NULL, &bytes_avail, NULL))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - PeekNamedPipe failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
    }

    if (bytes_avail <= 0)
    {
        return;
    }

    // If there's data available from the pipe, process it.
    char* buffer = new char[bytes_avail+1];
    DWORD num_read = 0;
    if (!ReadFile(m_pipe, buffer, bytes_avail, &num_read, NULL))
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - ReadFile failed (%d)"), __TFUNCTION__, __LINE__, GetLastError());
    }
    buffer[bytes_avail] = '\0';

    // Don't assume we receive complete lines in a read: "Data is written to an anonymous pipe
    // as a stream of bytes. This means that the parent process reading from a pipe cannot
    // distinguish between the bytes written in separate write operations, unless both the
    // parent and child processes use a protocol to indicate where the write operation ends."
    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa365782%28v=vs.85%29.aspx

    m_pipeBuffer.append(buffer);

    int start = 0;
    while (true)
    {
        int end = m_pipeBuffer.find("\n", start);
        if (end == string::npos)
        {
            m_pipeBuffer = m_pipeBuffer.substr(start);
            break;
        }
        string line = m_pipeBuffer.substr(start, end - start);
        HandleCoreProcessOutputLine(line.c_str());
        start = end + 1;
    }

    delete buffer;
}


void CoreTransport::HandleCoreProcessOutputLine(const char* line)
{
    // Notices are logged to diagnostics. Some notices are excluded from
    // diagnostics if they may contain private user data.
    bool logOutputToDiagnostics = true;

    // Parse output to extract data

    try
    {
        Json::Value notice;
        Json::Reader reader;
        if (!reader.parse(line, notice))
        {
            my_print(NOT_SENSITIVE, false, _T("%s: core notice JSON parse failed: %S"), __TFUNCTION__, reader.getFormattedErrorMessages().c_str());
            // This line was not JSON. It's not included in diagnostics
            // as we can't be sure it doesn't include user private data.
            return;
        }

        if (!notice.isObject())
        {
            // Ignore this line. The core internals may emit non-notice format
            // lines, and this confuses the JSON parser. This test filters out
            // those lines.
            return;
        }

        string noticeType = notice["noticeType"].asString();
        string timestamp = notice["timestamp"].asString();
        Json::Value data = notice["data"];

        // Let the UI know about it and decide if something needs to be shown to the user.
        if (noticeType != "Info")
        {
            UI_Notice(line);
        }

        if (noticeType == "Tunnels")
        {
            // This notice is received when tunnels are connected and disconnected.
            int count = data["count"].asInt();
            if (count == 0)
            {
                if (m_hasEverConnected && m_reconnectStateReceiver)
                {
                    m_reconnectStateReceiver->SetReconnecting();
                }
                m_isConnected = false;
            }
            else if (count == 1)
            {
                if (m_hasEverConnected && m_reconnectStateReceiver)
                {
                    m_reconnectStateReceiver->SetReconnected();
                }
                m_isConnected = true;
                m_hasEverConnected = true;
            }
        }
        else if (noticeType == "ClientUpgradeAvailable")
        {
            string version = data["version"].asString();
            m_sessionInfo.SetUpgradeVersion(version.c_str());
        }
        else if (noticeType == "Homepage")
        {
            string url = data["url"].asString();
            m_sessionInfo.SetHomepage(url.c_str());
        }
        else if (noticeType == "ListeningSocksProxyPort")
        {
            int port = data["port"].asInt();
            m_localSocksProxyPort = port;
        }
        else if (noticeType == "ListeningHttpProxyPort")
        {
            int port = data["port"].asInt();
            m_localHttpProxyPort = port;

            // In this special case, we're running the core solely to
            // use its url proxy and we do not expect to connect to a
            // tunnel. So ensure that TransportConnectHelper() returns
            // once the url proxy is running.
            if (RequestingUrlProxyWithoutTunnel())
            {
                m_isConnected = true;
            }
        }
        else if (noticeType == "SocksProxyPortInUse")
        {
            int port = data["port"].asInt();
            my_print(NOT_SENSITIVE, false, _T("SOCKS proxy port not available: %d"), port);
            // Don't try to reconnect with the same configuration
            throw TransportFailed(false);
        }
        else if (noticeType == "HttpProxyPortInUse")
        {
            int port = data["port"].asInt();
            my_print(NOT_SENSITIVE, false, _T("HTTP proxy port not available: %d"), port);
            // Don't try to reconnect with the same configuration
            throw TransportFailed(false);
        }
        else if (noticeType == "Untunneled")
        {
            string address = data["address"].asString();
            // SENSITIVE_LOG: "address" is site user is browsing
            my_print(SENSITIVE_LOG, false, _T("Untunneled: %S"), address.c_str());

            // Don't include in diagnostics as "address" is private user data
            logOutputToDiagnostics = false;
        }
        else if (noticeType == "SplitTunnelRegion")
        {
            string region = data["region"].asString();
            my_print(NOT_SENSITIVE, false, _T("Split Tunnel Region: %S"), region.c_str());
        }
        else if (noticeType == "UpstreamProxyError")
        {
            string message = data["message"].asString();

            if (message != m_lastUpstreamProxyErrorMessage)
            {
                // SENSITIVE_FORMAT_ARGS: "message" may contain address info that identifies user
                my_print(SENSITIVE_FORMAT_ARGS, false, _T("Upstream Proxy Error: %S"), message.c_str());

                // Don't repeatedly display the same error message, which may be emitted
                // many times as the core is making multiple attempts to establish tunnels.
                m_lastUpstreamProxyErrorMessage = message;
            }

            // Don't include in diagnostics as "message" may contain private user data
            logOutputToDiagnostics = false;

            // In this case, the user most likely input an incorrect upstream proxy
            // address or credential. So stop attempting to connect and let the user
            // handle the error message.
            // UPDATE:
            // Actually, there are many other conditions that might cause this case,
            // such as attempting to connect to disallowed ports through the proxy,
            // the proxy being temporarily overloaded, other temporary network conditions, etc.
            // So don't throw here.
            //throw TransportFailed(false);
            // TODO: The client should keep track of these notices and if it has not connected
            // within a certain amount of time and received many of these notices it should
            // suggest to the user that there might be a problem with the Upstream Proxy Settings.
        }
        else if (noticeType == "AvailableEgressRegions")
        {
            string regions = data["regions"].toStyledString();
            my_print(NOT_SENSITIVE, false, _T("Available egress regions: %S"), regions.c_str());
        }
    }
    catch (exception& e)
    {
        my_print(NOT_SENSITIVE, false, _T("%s: core notice JSON parse exception: %S"), __TFUNCTION__, e.what());
    }

    // Debug output, flag sensitive to exclude from feedback
    my_print(SENSITIVE_LOG, true, _T("core notice: %S"), line);

    // Add to diagnostics
    if (logOutputToDiagnostics)
    {
        AddDiagnosticInfoJson("CoreNotice", line);
    }
}


bool CoreTransport::DoPeriodicCheck()
{
    // Notes:
    // - used in both IWorkerThread::Thread and in local TransportConnectHelper
    // - ConsumeCoreProcessOutput accesses local state (m_pipeBuffer) without
    //   a mutex. This is safe because one thread (IWorkerThread::Thread) is currently
    //   making all the calls to DoPeriodicCheck()

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

            ConsumeCoreProcessOutput();

            return true;
        }
        else if (result == WAIT_OBJECT_0)
        {
            // The process has signalled -- which implies that it has died.
            // We'll consume the output anyway, as it might contain information
            // about why the process death occurred (such as port conflict).
            ConsumeCoreProcessOutput();
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
