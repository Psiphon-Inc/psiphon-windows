/*
 * Copyright (c) 2020, Psiphon Inc.
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

#define UPGRADE_EXE_NAME                     _T("psiphon3.exe.upgrade")

string GetUpstreamProxyAddress()
{
    // Note: only HTTP proxies and basic auth are currently supported

    if (Settings::SkipUpstreamProxy())
    {
        // Don't use an upstream proxy of any kind.
        return "";
    }

    string upstreamProxyAddress = Settings::UpstreamProxyFullHostname();
    if (upstreamProxyAddress.empty())
    {
        // There's no user-set upstream proxy, so use the native default proxy 
        // (that is, the one that was set before we tried to connect).
        auto proxyConfig = GetNativeDefaultProxyConfig();
        if (proxyConfig.HTTPEnabled())
        {
            upstreamProxyAddress = WStringToUTF8(proxyConfig.HTTPHostPortScheme());
        }
    }

    return upstreamProxyAddress;
}

bool WriteParameterFiles(const WriteParameterFilesIn& in, WriteParameterFilesOut& out)
{
    tstring dataStoreDirectory;
    if (!GetDataPath({ LOCAL_SETTINGS_APPDATA_SUBDIRECTORY }, true, dataStoreDirectory)) {
        my_print(NOT_SENSITIVE, false, _T("%s - GetDataPath failed for dataStoreDirectory (%d)"), __TFUNCTION__, GetLastError());
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
    config["ClientPlatform"] = GetClientPlatform();
    config["ClientVersion"] = CLIENT_VERSION;
    config["PropagationChannelId"] = PROPAGATION_CHANNEL_ID;
    config["SponsorId"] = SPONSOR_ID;
    config["RemoteServerListURLs"] = LoadJSONArray(REMOTE_SERVER_LIST_URLS_JSON);
    config["ObfuscatedServerListRootURLs"] = LoadJSONArray(OBFUSCATED_SERVER_LIST_ROOT_URLS_JSON);
    config["RemoteServerListSignaturePublicKey"] = REMOTE_SERVER_LIST_SIGNATURE_PUBLIC_KEY;
    config["ServerEntrySignaturePublicKey"] = SERVER_ENTRY_SIGNATURE_PUBLIC_KEY;
    config["DataRootDirectory"] = WStringToUTF8(shortDataStoreDirectory);
    config["MigrateDataStoreDirectory"] = WStringToUTF8(shortDataStoreDirectory);
    config["UseIndistinguishableTLS"] = true;
    config["DeviceRegion"] = WStringToUTF8(GetDeviceRegion());
    config["EmitDiagnosticNotices"] = true;
    config["EmitDiagnosticNetworkParameters"] = true;
    config["EmitServerAlerts"] = true;

    // Don't use an upstream proxy when in VPN mode. If the proxy is on a private network,
    // we may not be able to route to it. If the proxy is on a public network we prefer not
    // to use it for Psiphon requests (this assumes that this core transport has been created
    // as a temp tunnel or url proxy facilitator, since some underlying transport is already
    // providing whole system tunneling).
    if (!g_connectionManager.IsWholeSystemTunneled())
    {
        config["UpstreamProxyUrl"] = in.upstreamProxyAddress;
    }

    if (Settings::SplitTunnel())
    {
        config["EnableSplitTunnel"] = true;
    }

    if (Settings::DisableTimeouts())
    {
        config["NetworkLatencyMultiplierLambda"] = 0.1;
    }

    if (in.encodedAuthorizations != NULL) {
        config["Authorizations"] = in.encodedAuthorizations;
    }

    // TODO: Use a real network IDs
    // See https://github.com/Psiphon-Inc/psiphon-issues/issues/404
    config["NetworkID"] = "949F2E962ED7A9165B81E977A3B4758B";

    // Feedback
    config["FeedbackUploadURLs"] = LoadJSONArray(FEEDBACK_UPLOAD_URLS_JSON);
    config["FeedbackEncryptionPublicKey"] = FEEDBACK_ENCRYPTION_PUBLIC_KEY;

    // In temporary tunnel mode, only the specific server should be connected to,
    // and a handshake is not performed.
    // For example, in VPN mode, the temporary tunnel is used by the VPN mode to
    // perform its own handshake request to get a PSK.
    //
    // This same minimal setup is used in the RequestingUrlProxyWithoutTunnel mode,
    // although in this case we don't set a deadline to connect since we don't
    // expect to ever connect to a tunnel and we we want to allow the caller to
    // complete the (direct) url proxied request
    if (in.tempConnectServerEntry != NULL || in.requestingUrlProxyWithoutTunnel)
    {
        config["DisableApi"] = true;
        config["DisableRemoteServerListFetcher"] = true;
        string serverEntry = in.tempConnectServerEntry->ToString();
        config["TargetServerEntry"] =
            Hexlify((const unsigned char*)(serverEntry.c_str()), serverEntry.length());
        // Use whichever region the server entry is located in
        config["EgressRegion"] = "";

        if (in.requestingUrlProxyWithoutTunnel)
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

            config["DataRootDirectory"] = WStringToUTF8(shortTempDir);
            config["MigrateDataStoreDirectory"] = WStringToUTF8(shortTempDir);
        }
        else
        {
            config["EstablishTunnelTimeoutSeconds"] = TEMPORARY_TUNNEL_TIMEOUT_SECONDS;
        }

        out.oldClientUpgradeFilename.clear();
        out.newClientUpgradeFilename.clear();
    }
    else
    {
        config["EgressRegion"] = Settings::EgressRegion();

        unsigned int localHttpProxyPortSetting = Settings::LocalHttpProxyPort();
        unsigned int localSocksProxyPortSetting = Settings::LocalSocksProxyPort();
        if (localHttpProxyPortSetting > 0)
        {
            my_print(NOT_SENSITIVE, true, _T("Setting LocalHttpProxyPort to a user configured value"));
        }
        if (localSocksProxyPortSetting > 0)
        {
            my_print(NOT_SENSITIVE, true, _T("Setting LocalSocksProxyPort to a user configured value"));
        }
        config["LocalHttpProxyPort"] = localHttpProxyPortSetting;
        config["LocalSocksProxyPort"] = localSocksProxyPortSetting;

        if (Settings::ExposeLocalProxiesToLAN())
        {
            config["ListenInterface"] = "any";
            my_print(NOT_SENSITIVE, true, _T("Setting ListenInterface to any"));
        }

        auto remoteServerListFilename = filesystem::path(dataStoreDirectory)
            .append(LOCAL_SETTINGS_APPDATA_REMOTE_SERVER_LIST_FILENAME);
        config["MigrateRemoteServerListDownloadFilename"] = WStringToUTF8(remoteServerListFilename.wstring());

        tstring oslDownloadDirectory;
        if (GetDataPath({ LOCAL_SETTINGS_APPDATA_SUBDIRECTORY, _T("osl") }, false, oslDownloadDirectory)) {
            config["MigrateObfuscatedServerListDownloadDirectory"] = WStringToUTF8(oslDownloadDirectory);
        }

        out.oldClientUpgradeFilename = filesystem::path(shortDataStoreDirectory).append(UPGRADE_EXE_NAME);

        config["MigrateUpgradeDownloadFilename"] = WStringToUTF8(out.oldClientUpgradeFilename);
        config["UpgradeDownloadURLs"] = LoadJSONArray(UPGRADE_URLS_JSON);
        config["UpgradeDownloadClientVersionHeader"] = string("x-amz-meta-psiphon-client-version");

        // Newer versions of tunnel-core download the upgrade file to its own data directory. Both oldClientUpgradeFilename and
        // newClientUpgradeFilename should be deleted when Psiphon starts if they exist.
        // TODO: when we switch to using tunnel-core as a library instead of a subprocess then we can call UpgradeDownloadFilePath()
        // rather than constructing the path here.
        out.newClientUpgradeFilename = filesystem::path(shortDataStoreDirectory).append(_T("ca.psiphon.PsiphonTunnel.tunnel-core")).append(_T("upgrade"));
    }

    ostringstream configDataStream;
    Json::FastWriter jsonWriter;
    configDataStream << jsonWriter.write(config);

    auto configPath = filesystem::path(dataStoreDirectory);
    configPath.append(in.configFilename);
    out.configFilePath = configPath;

    if (!WriteFile(out.configFilePath, configDataStream.str()))
    {
        my_print(NOT_SENSITIVE, false, _T("%s - write config file failed (%d)"), __TFUNCTION__, GetLastError());
        return false;
    }

    // RequestingUrlProxyWithoutTunnel mode omits the server list file, since
    // it's not trying to establish a tunnel.

    if (!in.requestingUrlProxyWithoutTunnel)
    {
        auto serverListPath = filesystem::path(dataStoreDirectory)
            .append(LOCAL_SETTINGS_APPDATA_SERVER_LIST_FILENAME);
        out.serverListFilename = serverListPath;

        if (!WriteFile(out.serverListFilename, EMBEDDED_SERVER_LIST))
        {
            my_print(NOT_SENSITIVE, false, _T("%s - write server list file failed (%d)"), __TFUNCTION__, GetLastError());
            return false;
        }
    }

    return true;
}
