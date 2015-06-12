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
    HTTPSRequest(bool silentMode=false);
    virtual ~HTTPSRequest();

    bool MakeRequest(
        const TCHAR* serverAddress,
        int serverWebPort,
        const string& webServerCertificate,
        const TCHAR* requestPath,
        string& response,
        const StopInfo& stopInfo,
        bool usePsiphonLocalProxy,
        bool failoverToURLProxy=false,
        LPCWSTR additionalHeaders=NULL,
        LPVOID additionalData=NULL,
        DWORD additionalDataLength=0,
        LPCWSTR httpVerb=NULL);

private:
    void SetClosedEvent() {SetEvent(m_closedEvent);}
    void SetRequestSuccess() {m_requestSuccess = true;}
      bool ValidateServerCert(PCCERT_CONTEXT pCert);
    void AppendResponse(const string& responseData);

    bool MakeRequestWithURLProxyOption(
        const TCHAR* serverAddress,
        int serverWebPort,
        const string& webServerCertificate,
        const TCHAR* requestPath,
        string& response,
        const StopInfo& stopInfo,
        bool usePsiphonLocalProxy,
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
    string m_response;
};
