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

#include "worker_thread.h"
#include "transport.h"
#include "transport_registry.h"
#include "usersettings.h"

#pragma once

/*
 * Utilities used in conjunction with the psiphon-tunnel-core executable.
 */

/**
Returns the upstream HTTP proxy address configured in settings. If the user,
has not configured an upstream proxy then use the native default proxy if it
is configured with an HTTP proxy. If no upstream proxy is configured, then
returns an empty string.
*/
string GetUpstreamProxyAddress();

// Input arguments to WriteParameterFiles
struct WriteParameterFilesIn {
    bool requestingUrlProxyWithoutTunnel;
    string configFilename;
    string upstreamProxyAddress;
    Json::Value encodedAuthorizations;
    const ServerEntry* tempConnectServerEntry;
};

// Ouput information from WriteParameterFiles
struct WriteParameterFilesOut {
    tstring configFilePath;
    tstring serverListFilename;
    tstring oldClientUpgradeFilename;
    tstring newClientUpgradeFilename;
};

/**
Writes files which need to be supplied to the psiphon-tunnel-core executable,
as command line flags, when it is spawned as a subprocess.
Returns true if the files were successfully paved, otherwise returns false.
*/
bool WriteParameterFiles(const WriteParameterFilesIn& in, WriteParameterFilesOut& out);
