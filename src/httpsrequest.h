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

using namespace std;

class HTTPSRequest
{
public:
    HTTPSRequest(void);
    virtual ~HTTPSRequest(void);
    bool MakeRequest(
        const bool& cancel,
        const TCHAR* serverAddress,
        int serverWebPort,
        const string& webServerCertificate,
        const TCHAR* requestPath,
        string& response,
        bool useProxy=false,
        LPCWSTR additionalHeaders=NULL,
        LPVOID additionalData=NULL,
        DWORD additionalDataLength=0);

    // NOTE: these member functions could be private if the WinHttpStatusCallback
    // function is a static member or friend
    void SetClosedEvent(void) {SetEvent(m_closedEvent);}
    void SetRequestSuccess(void) {m_requestSuccess = true;}
  	bool ValidateServerCert(PCCERT_CONTEXT pCert);
    void AppendResponse(const string& responseData);

private:
    HANDLE m_mutex;
    HANDLE m_closedEvent;
    bool m_requestSuccess;
    string m_expectedServerCertificate;
    string m_response;
};
