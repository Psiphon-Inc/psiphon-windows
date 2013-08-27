/*
 * Copyright (c) 2012, Psiphon Inc.
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

#pragma once

#include "stopsignal.h"

class ITransport;
class SessionInfo;
struct ServerEntry;


class ServerRequest
{
public:
    ServerRequest();
    virtual ~ServerRequest();

    enum ReqLevel
    {
        // Do everything to try to make the request: current transport, HTTPS 
        // ports, temp tunnels.
        FULL,

        // Don't make temp tunnels when trying to make the request. Only 
        // current transport and HTTPS ports.
        NO_TEMP_TUNNEL,

        // Don't try to make the request if there's no currently connected
        // transport.
        ONLY_IF_TRANSPORT
    };

    // Throws stop signal.
    static bool MakeRequest(
        ReqLevel reqLevel,
        const ITransport* currentTransport,
        const SessionInfo& sessionInfo,
        const TCHAR* requestPath,
        string& o_response,
        const StopInfo& stopInfo,
        LPCWSTR additionalHeaders=NULL,
        LPVOID additionalData=NULL,
        DWORD additionalDataLength=0);

    static bool ServerHasRequestCapabilities(const ServerEntry& serverEntry);

private:
    static void GetTempTransports(
                const ServerEntry& serverEntry,
                vector<boost::shared_ptr<ITransport>>& o_tempTransports);
};
