/*
 * Copyright (c) 2011, Psiphon Inc.
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

#include <string>
#include <WinCrypt.h>
#include <Winhttp.h>
#include "stopsignal.h"


using namespace std;

class HTTPSRequest
{
public:
    struct Response {
        int code;
        string body;
        map<string, vector<string>> headers;

        Response() : code(-1) {}
    };

    // HTTP status code for "OK". This will save us using the magic number everywhere,
    // but we're not going to provide aliases for any other codes.
    static constexpr int OK = 200;

public:
    HTTPSRequest(bool silentMode=false);
    virtual ~HTTPSRequest();

    enum class PsiphonProxy {
        DONT_USE = 0,
        USE,
        REQUIRE
    };

    // requestPath must include the path, the query parameters, and the anchor, if any.
    // additionalHeaders must be valid RFC2616, including CRLF separator between multiple items.
    // Returns true if the request succeeded, regardless of response status code. Caller
    // should check the response.code.
    // Throws StopSignal::StopException if stop was signaled.
    bool MakeRequest(
        const TCHAR* serverAddress,
        int serverWebPort,
        const string& webServerCertificate,
        const TCHAR* requestPath,
        const StopInfo& stopInfo,
        PsiphonProxy usePsiphonLocalProxy,
        HTTPSRequest::Response& response,
        bool failoverToURLProxy=false,
        LPCWSTR additionalHeaders=NULL,
        LPVOID additionalData=NULL,
        DWORD additionalDataLength=0,
        LPCWSTR httpVerb=NULL);

private:
    void SetClosedEvent() {SetEvent(m_closedEvent);}
    void SetRequestSuccess() {m_requestSuccess = true;}
    bool ValidateServerCert(PCCERT_CONTEXT pCert);
    void ResponseAppendBody(const string& responseData);
    void ResponseSetCode(int code);
    void ResponseSetHeaders(const std::map<std::string, std::vector<std::string>>& headers);

    bool MakeRequestWithURLProxyOption(
        const TCHAR* serverAddress,
        int serverWebPort,
        const string& webServerCertificate,
        const TCHAR* requestPath,
        const StopInfo& stopInfo,
        PsiphonProxy usePsiphonLocalProxy,
        HTTPSRequest::Response& response,
        bool useURLProxy,
        LPCWSTR additionalHeaders,
        LPVOID additionalData,
        DWORD additionalDataLength,
        LPCWSTR httpVerb);

    friend void CALLBACK WinHttpStatusCallback(
                            HINTERNET hRequest,
                            DWORD_PTR dwContext,
                            DWORD dwInternetStatus,
                            LPVOID lpvStatusInformation,
                            DWORD dwStatusInformationLength);

private:
    bool m_silentMode;
    HANDLE m_mutex;
    HANDLE m_closedEvent;
    bool m_requestSuccess;
    string m_expectedServerCertificate;
    Response m_response;
};
