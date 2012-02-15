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

/*
The purpose of ServerRequest is to wrap the various ways to make a HTTPS request
to the server, which will depend on what state we're in, what transports are 
available, etc.

Design assumptions:
- All transports will run a local proxy.
  - This is true at this time, but it's imaginable that it could change in the
    future. For now, though, when a transport is up we will alway route requests
    through the local proxy.

There are two basic states we can be in: 1) a transport is connected; 
and 2) no transport is connected. 

If a transport is connected, the request method is simple:
- Connect via the local proxy, using HTTPS on port 8080.

If a transport is not connected, the request method fails over among multiple
methods:

1. Directly
Connect directly with HTTPS. Fail over among specific ports (right now those
are 8080 and 443).

2. Via transport
Some transports (e.g., SSH) have all necessary connection information 
contained in their local ServerEntry; no separate handshake is required to
connect with these transports. If direct connection attempts fail, we will
fail over to attempting to connect each of these types of transports and 
proxying our request through them.
*/

#include "stdafx.h"
#include "server_request.h"


ServerRequest::ServerRequest()
{
}

bool ServerRequest::MakeRequest(
        const bool& cancel,
        const TCHAR* serverAddress,
        int serverWebPort,
        const string& webServerCertificate,
        const TCHAR* requestPath,
        string& response,
        int proxyPort/*=0*/,
        LPCWSTR additionalHeaders/*=NULL*/,
        LPVOID additionalData/*=NULL*/,
        DWORD additionalDataLength/*=0*/)
{
}
