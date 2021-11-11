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
#include "authenticated_data_package.h"
#include "psiphon_tunnel_core_utilities.h"

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
      m_localSocksProxyPort(AUTOMATICALLY_ASSIGNED_PORT_NUMBER),
      m_localHttpProxyPort(AUTOMATICALLY_ASSIGNED_PORT_NUMBER),
      m_hasEverConnected(false),
      m_isConnected(false),
      m_clientUpgradeDownloadHandled(false)
{
}


CoreTransport::~CoreTransport()
{
    (void)Cleanup();
    IWorkerThread::Stop();
}


bool CoreTransport::Cleanup()
{
    m_psiphonTunnelCore = nullptr;
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


bool CoreTransport::SupportsAuthorizations() const
{
    return true;
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
            my_print(NOT_SENSITIVE, false, _T("%s connection failed"), GetTransportDisplayName().c_str());
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
    return m_tempConnectServerEntry != NULL &&
        m_tempConnectServerEntry->serverAddress.empty();
}


void CoreTransport::TransportConnectHelper()
{
    assert(m_systemProxySettings != NULL);

    Json::Value encodedAuthorizations = Json::Value(Json::arrayValue);
    if (m_authorizationsProvider) {
        for (const auto& auth : m_authorizationsProvider->GetAuthorizations()) {
            encodedAuthorizations.append(auth.encoded);
            m_authorizationIDs.push_back(auth.id);
        }
    }

    WriteParameterFilesOut out;
    WriteParameterFilesIn in;
    in.requestingUrlProxyWithoutTunnel = RequestingUrlProxyWithoutTunnel();
    if (in.requestingUrlProxyWithoutTunnel) {
        // RequestingUrlProxyWithoutTunnel mode has a distinct config file so that
        // it won't conflict with a standard CoreTransport which may already be
        // running.
        // TODO: there's still a remote chance that concurrently spawned url proxy
        // instances could clobber each other's config file?
        in.configFilename = WStringToUTF8(LOCAL_SETTINGS_APPDATA_URL_PROXY_CONFIG_FILENAME);
    }
    else {
        in.configFilename = WStringToUTF8(LOCAL_SETTINGS_APPDATA_CONFIG_FILENAME);
    }

    in.upstreamProxyAddress = GetUpstreamProxyAddress();
    in.encodedAuthorizations = encodedAuthorizations;
    in.tempConnectServerEntry = m_tempConnectServerEntry;

    if (!WriteParameterFiles(in, out))
    {
        throw TransportFailed();
    }

    // Once a new upgrade has been paved, CoreTransport should never restart without the actual application restarting.
    // If there is a pending upgrade, when disconnect/connect is pressed (or a new region is chosen, upstream proxy settings change, etc.)
    // the application is killed and relaunched with the new image. The upgrade package is not immediately deleted on a successful pave
    // to prevent re-download. When CoreTransport starts, we assume it's with the new binary, and deleting the .upgrade file is safe.
    if (!out.oldClientUpgradeFilename.empty() && !DeleteFile(out.oldClientUpgradeFilename.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
    {
        my_print(NOT_SENSITIVE, false, _T("Failed to delete previously applied upgrade package! Please report this error."));
    }

    if (!out.newClientUpgradeFilename.empty() && !DeleteFile(out.newClientUpgradeFilename.c_str()))
    {
        int error = GetLastError();
        if ((error != ERROR_FILE_NOT_FOUND) && (error != ERROR_PATH_NOT_FOUND))
        {
            my_print(NOT_SENSITIVE, false, _T("Failed to delete previously applied upgrade package! Please report this error."));
        }
    }

    // Run core process; it will begin establishing a tunnel

    if (!SpawnCoreProcess(out.configFilePath, out.serverListFilename))
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
    m_systemProxySettings->SetHttpProxyPort(m_localHttpProxyPort);
    m_systemProxySettings->SetHttpsProxyPort(m_localHttpProxyPort);

    vector<tstring> ipAddresses;
    GetLocalIPv4Addresses(ipAddresses);
    if (Settings::ExposeLocalProxiesToLAN() && ipAddresses.size() > 0)
    {
        for (const auto& ipAddress : ipAddresses)
        {
            my_print(SENSITIVE_FORMAT_ARGS, false, _T("SOCKS proxy is running on %s port %d."), ipAddress.c_str(), m_localSocksProxyPort);
            my_print(SENSITIVE_FORMAT_ARGS, false, _T("HTTP proxy is running on %s port %d."), ipAddress.c_str(), m_localHttpProxyPort);
        }
    }
    else
    {
        my_print(NOT_SENSITIVE, false, _T("SOCKS proxy is running on localhost port %d."), m_localSocksProxyPort);
        my_print(NOT_SENSITIVE, false, _T("HTTP proxy is running on localhost port %d."), m_localHttpProxyPort);
    }
}


bool CoreTransport::SpawnCoreProcess(const tstring& configFilename, const tstring& serverListFilename)
{
    tstringstream commandLineFlags;
    tstring exePath;

    if (RequestingUrlProxyWithoutTunnel())
    {
        // In RequestingUrlProxyWithoutTunnel mode, we allow for multiple instances
        // so we don't fail extract if the file already exists -- and don't try to
        // kill any associated process holding a lock on it.
        if (!ExtractExecutable(
                IDR_PSIPHON_TUNNEL_CORE_EXE, URL_PROXY_EXE_NAME, exePath, true))
        {
            return false;
        }
    }
    else
    {
        if (!ExtractExecutable(
                IDR_PSIPHON_TUNNEL_CORE_EXE, EXE_NAME, exePath))
        {
            return false;
        }
    }

    commandLineFlags <<  _T(" --config \"") << configFilename << _T("\"");

    if (!RequestingUrlProxyWithoutTunnel())
    {
        commandLineFlags << _T(" --serverList \"") << serverListFilename << _T("\"");
    }

    m_psiphonTunnelCore = make_unique<PsiphonTunnelCore>(this, exePath);
    if (!m_psiphonTunnelCore->SpawnSubprocess(commandLineFlags.str())) {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - SpawnSubprocess failed"), __TFUNCTION__, __LINE__);
        return false;
    }

    if (!m_psiphonTunnelCore->CloseInputPipes()) {
        my_print(NOT_SENSITIVE, false, _T("%s - failed to close input pipes"), __TFUNCTION__);
        return false;
    }

    return true;
}


bool CoreTransport::ValidateAndPaveUpgrade(const tstring& clientUpgradeFilename) {
    bool processingSuccessful = false;

    HANDLE hFile = CreateFile(
        clientUpgradeFilename.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL, // default security
        OPEN_EXISTING, // existing file only
        FILE_ATTRIBUTE_NORMAL,
        NULL); // no attr. template

    if (hFile == INVALID_HANDLE_VALUE) {
        my_print(NOT_SENSITIVE, false, _T("%s: Could not get a valid file handle: %d."), __TFUNCTION__, GetLastError());
    }
    else {
        DWORD dwFileSize = GetFileSize(hFile, NULL);
        unique_ptr<BYTE> inBuffer(new BYTE[dwFileSize]);
        if (!inBuffer) {
            processingSuccessful = false;
            my_print(NOT_SENSITIVE, false, _T("%s: Could not allocate an input buffer."), __TFUNCTION__);
        }
        else {
            DWORD dwBytesToRead = dwFileSize;
            DWORD dwBytesRead = 0;
            OVERLAPPED stOverlapped = { 0 };
            string downloadFileString;

            if (TRUE == ReadFile(hFile, inBuffer.get(), dwBytesToRead, &dwBytesRead, NULL)) {
                if (verifySignedDataPackage(
                    UPGRADE_SIGNATURE_PUBLIC_KEY,
                    (const char *)inBuffer.get(),
                    dwFileSize,
                    true, // gzip compressed
                    downloadFileString))
                {
                    // Data in the package is Base64 encoded
                    downloadFileString = Base64Decode(downloadFileString);

                    if (downloadFileString.length() > 0) {
                        m_upgradePaver->PaveUpgrade(downloadFileString);
                    }

                    processingSuccessful = true;
                }
            }
        }

        CloseHandle(hFile);
    }

    if (!processingSuccessful) {
        // Bad package. Log and continue.
        my_print(NOT_SENSITIVE, false, _T("%s: Upgrade package verification failed! Please report this error."), __TFUNCTION__);

        // If the file isn't working and we think we have the complete file,
        // there may be corrupt bytes. So delete it and next time we'll start over.
        // NOTE: this means if the failure was due to not enough free space
        // to write the extracted file... we still re-download.
        if (!DeleteFile(clientUpgradeFilename.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
            my_print(NOT_SENSITIVE, false, _T("Failed to delete invalid upgrade package! Please report this error."));
        }
    }

    // If the extract and verify succeeded, don't delete it to prevent re-downloading.
    // TransportConnect will check for the existence of this file and delete it itself.
    return processingSuccessful;
}


void CoreTransport::HandlePsiphonTunnelCoreNotice(const string& noticeType, const string& timestamp, const Json::Value& data)
{
    if (noticeType == "Tunnels")
    {
        // This notice is received when tunnels are connected and disconnected.
        int count = data["count"].asInt();
        if (count == 0)
        {
            if (m_hasEverConnected && m_reconnectStateReceiver && !m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
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
    else if (noticeType == "ClientUpgradeDownloaded" && m_upgradePaver != NULL && m_clientUpgradeDownloadHandled == false) {
        m_clientUpgradeDownloadHandled = true;

        my_print(NOT_SENSITIVE, false, _T("A client upgrade has been downloaded..."));
        if (!ValidateAndPaveUpgrade(UTF8ToWString(data["filename"].asString()))) {
            m_clientUpgradeDownloadHandled = false;
        }
        my_print(NOT_SENSITIVE, false, _T("Psiphon has been updated. The new version will launch the next time Psiphon starts."));
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
        my_print(NOT_SENSITIVE, true, _T("Available egress regions: %S"), regions.c_str());
        // Processing this is left to main.js
    }
    else if (noticeType == "ActiveAuthorizationIDs")
    {
        string authIDs = data["IDs"].toStyledString();
        my_print(NOT_SENSITIVE, true, _T("Active Authorization IDs: %S"), authIDs.c_str());

        vector<string> activeAuthorizationIDs, inactiveAuthorizationIDs;
        for (const auto& activeAuthID : data["IDs"])
        {
            activeAuthorizationIDs.push_back(activeAuthID.asString());
        }

        // Figure out which of the authorizations we provided to the server were and were not active.
        for (const auto& authID : m_authorizationIDs)
        {
            if (std::find(activeAuthorizationIDs.cbegin(), activeAuthorizationIDs.cend(), authID) == activeAuthorizationIDs.cend())
            {
                inactiveAuthorizationIDs.push_back(authID);
            }
        }

        if (m_authorizationsProvider) {
            m_authorizationsProvider->ActiveAuthorizationIDs(activeAuthorizationIDs, inactiveAuthorizationIDs);
        }
    }
    else if (noticeType == "ClientRegion")
    {
        string region = data["region"].asString();
        my_print(NOT_SENSITIVE, true, _T("Client region: %S"), region.c_str());
        psicash::Lib::_().UpdateClientRegion(region);
    }
    else if (noticeType == "SplitTunnelRegions")
    {
        string regions = data["regions"].toStyledString();
        my_print(NOT_SENSITIVE, false, _T("Split Tunnel Regions: %S"), regions.c_str());
    }
    else if (noticeType == "TrafficRateLimits")
    {
        string speed = data["downstreamBytesPerSecond"].toStyledString();
        my_print(NOT_SENSITIVE, true, _T("Traffic rate downstream limit: %S"), speed.c_str());
        // Processing this is left to main.js
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

    try {
        DWORD status = m_psiphonTunnelCore->Status();
        if (status == SUBPROCESS_STATUS_RUNNING) {
            if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
            {
                throw Abort();
            }

            m_psiphonTunnelCore->ConsumeSubprocessOutput();

            return true;
        }
        else if (status == SUBPROCESS_STATUS_EXITED) {
            // The process has signalled -- which implies that it has died.
            // We'll consume the output anyway, as it might contain information
            // about why the process death occurred (such as port conflict).
            m_psiphonTunnelCore->ConsumeSubprocessOutput();
            return false;
        }
        else if (status == SUBPROCESS_STATUS_NO_PROCESS) {
            return false;
        }
        else {
            my_print(NOT_SENSITIVE, false, _T("%s - unexpected subprocess status (%d)"), __TFUNCTION__, status);
            return false;
        }
    }
    catch (Subprocess::Error& error) {
        my_print(NOT_SENSITIVE, false, _T("%s - caught Subprocess::Error: %s"), __TFUNCTION__, error.GetMessage().c_str());
    }

    return false;
}
