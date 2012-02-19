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

#include "stdafx.h"
#include "httpsrequest.h"
#include "psiclient.h"
#include <Winhttp.h>
#include <WinCrypt.h>
#include "config.h"
#include "embeddedvalues.h"


// NOTE: this code depends on built-in Windows crypto services
// How do export restrictions impact general availability of crypto services?
// http://technet.microsoft.com/en-us/library/cc962093.aspx
// (This is also a concern for the VPN client we configure.)

#pragma comment (lib, "crypt32.lib")


class AutoHINTERNET
{
public:
    AutoHINTERNET(HINTERNET handle) {m_handle = handle;}
    ~AutoHINTERNET() { this->WinHttpCloseHandle(); }
    operator HINTERNET() {return m_handle;}
    void WinHttpCloseHandle() { if (m_handle != NULL) ::WinHttpCloseHandle(m_handle); m_handle = NULL; }
private:
    HINTERNET m_handle;
};


HTTPSRequest::HTTPSRequest()
    : m_closedEvent(NULL)
{
    m_mutex = CreateMutex(NULL, FALSE, 0);
}

HTTPSRequest::~HTTPSRequest()
{
    // In case object is destroyed while callback is outstanding, wait
    if (m_closedEvent != NULL)
    {
        WaitForSingleObject(m_closedEvent, INFINITE);
        CloseHandle(m_closedEvent);
        m_closedEvent = NULL;
    }

    CloseHandle(m_mutex);
}

void CALLBACK WinHttpStatusCallback(
                HINTERNET hRequest,
                DWORD_PTR dwContext,
                DWORD dwInternetStatus,
                LPVOID lpvStatusInformation,
                DWORD dwStatusInformationLength)
{
    // From: http://msdn.microsoft.com/en-us/library/windows/desktop/aa384068%28v=vs.85%29.aspx
    // "...it is possible in WinHTTP for a notification to occur before a context 
    // value is set. If the callback function receives a notification before the 
    // context value is set, the application must be prepared to receive NULL in 
    // the dwContext parameter of the callback function."
    if (dwContext == NULL)
    {
        my_print(true, _T("%s: received no context; thread exiting"), __TFUNCTION__);
        return;
    }

    HTTPSRequest* httpRequest = (HTTPSRequest*)dwContext;
    CERT_CONTEXT *pCert = {0};
    DWORD dwStatusCode;
    DWORD dwLen;
    LPVOID pBuffer = NULL;

    my_print(true, _T("HTTPS request... (%d)"), dwInternetStatus);

    switch (dwInternetStatus)
    {
    case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
        // This is ALWAYS the last notification; once it sets closed signal
        // it's safe to deallocate the parent httpRequest
        httpRequest->SetClosedEvent();
        break;
    case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:

        // NOTE: from experimentation, this is really the earliest we can inject our custom server cert validation.
        // As far as we know, this is before any data is sent over the SSL connection, so it's soon enough.
        // E.g., we tried to verify the cert earlier but:
        // WinHttpQueryOption(WINHTTP_OPTION_SERVER_CERT_CONTEXT) gives ERROR_WINHTTP_INCORRECT_HANDLE_STATE
        // during WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER...

        // Validate server certificate (before requesting)

        dwLen = sizeof(pCert);
        if (!WinHttpQueryOption(
                    hRequest,
                    WINHTTP_OPTION_SERVER_CERT_CONTEXT,
                    &pCert,
                    &dwLen)
            || NULL == pCert)
        {
            my_print(false, _T("WinHttpQueryOption failed (%d)"), GetLastError());
            WinHttpCloseHandle(hRequest);
            return;
        }

        if (!httpRequest->ValidateServerCert((PCCERT_CONTEXT)pCert))
        {
            CertFreeCertificateContext(pCert);
            my_print(false, _T("ValidateServerCert failed"));
            // Close request handle immediately to prevent sending of data
            WinHttpCloseHandle(hRequest);
            return;
        }
        break;
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:

        // Start downloding response

        if (!WinHttpReceiveResponse(hRequest, NULL))
        {
            my_print(false, _T("WinHttpReceiveResponse failed (%d)"), GetLastError());
            WinHttpCloseHandle(hRequest);
            return;
        }
        break;
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:

        // Check for HTTP status 200 OK

        dwLen = sizeof(dwStatusCode);
        if (!WinHttpQueryHeaders(
                        hRequest, 
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        NULL, 
                        &dwStatusCode, 
                        &dwLen, 
                        NULL))
        {
            my_print(false, _T("WinHttpQueryHeaders failed (%d)"), GetLastError());
            WinHttpCloseHandle(hRequest);
            return;
        }

        if (200 != dwStatusCode)
        {
            my_print(false, _T("Bad HTTP GET request status code: %d"), dwStatusCode);
            WinHttpCloseHandle(hRequest);
            return;
        }

        if (!WinHttpQueryDataAvailable(hRequest, 0))
        {
            my_print(false, _T("WinHttpQueryDataAvailable failed (%d)"), GetLastError());
            WinHttpCloseHandle(hRequest);
            return;
        }
        break;
    case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:

        // Read available response data

        dwLen = *((DWORD*)lpvStatusInformation);

        if (dwLen == 0)
        {
            // Read response is complete

            httpRequest->SetRequestSuccess();
            WinHttpCloseHandle(hRequest);
            return;
        }

        pBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwLen);

        if (!pBuffer)
        {
            WinHttpCloseHandle(hRequest);
            return;
        }
    
        if (!WinHttpReadData(hRequest, pBuffer, dwLen, &dwLen))
        {
            my_print(false, _T("WinHttpReadData failed (%d)"), GetLastError());
            HeapFree(GetProcessHeap(), 0, pBuffer);
            WinHttpCloseHandle(hRequest);
            return;
        }

        // NOTE: response data may be binary; some relevant comments here...
        // http://stackoverflow.com/questions/441203/proper-way-to-store-binary-data-with-c-stl

        httpRequest->AppendResponse(string(string::const_pointer(pBuffer), string::const_pointer((char*)pBuffer + dwLen)));

        HeapFree(GetProcessHeap(), 0, pBuffer);

        // Check for more data

        if (!WinHttpQueryDataAvailable(hRequest, 0))
        {
            my_print(false, _T("WinHttpQueryDataAvailable failed (%d)"), GetLastError());
            WinHttpCloseHandle(hRequest);
            return;
        }
        break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
        // Get error value as per http://msdn.microsoft.com/en-us/library/aa383917%28v=VS.85%29.aspx
        if (ERROR_WINHTTP_OPERATION_CANCELLED != ((WINHTTP_ASYNC_RESULT*)lpvStatusInformation)->dwError)
        {
            my_print(false, _T("HTTP request error (%d, %d)"),
                     ((WINHTTP_ASYNC_RESULT*)lpvStatusInformation)->dwResult,
                     ((WINHTTP_ASYNC_RESULT*)lpvStatusInformation)->dwError);
        }
        WinHttpCloseHandle(hRequest);
        break;
    case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
        my_print(false, _T("HTTP secure failure (%d)"), lpvStatusInformation);
        WinHttpCloseHandle(hRequest);
        break;
    default:
        // No action on other events
        break;
    }
}

bool HTTPSRequest::MakeRequest(
        const bool& cancel,
        const TCHAR* serverAddress,
        int serverWebPort,
        const string& webServerCertificate,
        const TCHAR* requestPath,
        string& response,
        bool useLocalProxy/*=true*/,
        LPCWSTR additionalHeaders/*=NULL*/,
        LPVOID additionalData/*=NULL*/,
        DWORD additionalDataLength/*=0*/)
{
    DWORD dwFlags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                    SECURITY_FLAG_IGNORE_UNKNOWN_CA;

    tstring proxy;
    if (useLocalProxy)
    {
        proxy = GetSystemDefaultHTTPSProxy();
    }

    AutoHINTERNET hSession =
                WinHttpOpen(
                    _T("Mozilla/4.0 (compatible; MSIE 5.22)"),
                    proxy.length() ? WINHTTP_ACCESS_TYPE_NAMED_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    proxy.length() ? proxy.c_str() : WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS,
                    WINHTTP_FLAG_ASYNC);

    if (NULL == hSession)
    {
        my_print(false, _T("WinHttpOpen failed (%d)"), GetLastError());
        return false;
    }

    AutoHINTERNET hConnect =
            WinHttpConnect(
                hSession,
                serverAddress,
                serverWebPort,
                0);

    if (NULL == hConnect)
    {
        my_print(false, _T("WinHttpConnect failed (%d)"), GetLastError());
        return false;
    }

    // Note: when certificate is empty, not using HTTPS

    AutoHINTERNET hRequest =
            WinHttpOpenRequest(
                    hConnect,
                    additionalData ? _T("POST") : _T("GET"),
                    requestPath,
                    NULL,
                    WINHTTP_NO_REFERER,
                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                    WINHTTP_FLAG_SECURE);

    if (NULL == hRequest)
    {
        my_print(false, _T("WinHttpOpenRequest failed (%d)"), GetLastError());
        return false;
    }

    if (FALSE == WinHttpSetOption(
                    hRequest,
                    WINHTTP_OPTION_SECURITY_FLAGS,
                    &dwFlags,
                    sizeof(DWORD)))
    {
        my_print(false, _T("WinHttpSetOption failed (%d)"), GetLastError());
        return false;
    }

    if (WINHTTP_INVALID_STATUS_CALLBACK == WinHttpSetStatusCallback(
                                                hRequest,
                                                WinHttpStatusCallback,
                                                WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS,
                                                NULL))
    {
        my_print(false, _T("WinHttpSetStatusCallback failed (%d)"), GetLastError());
        return false;
    }

    // Kick off the asynchronous processing

    // Must use a manual event: multiple things wait on the same event
    assert(m_closedEvent == NULL);
    m_closedEvent = CreateEvent(NULL, TRUE, FALSE, 0);

    m_expectedServerCertificate = webServerCertificate;
    m_requestSuccess = false;
    m_response = "";

    if (FALSE == WinHttpSendRequest(
                    hRequest,
                    additionalHeaders ? additionalHeaders : WINHTTP_NO_ADDITIONAL_HEADERS,
                    additionalHeaders ? wcslen(additionalHeaders) : 0,
                    additionalData ? additionalData : WINHTTP_NO_REQUEST_DATA,
                    additionalDataLength,
                    additionalDataLength,
                    (DWORD_PTR)this))
    {
        CloseHandle(m_closedEvent);
        m_closedEvent = NULL;
        my_print(false, _T("WinHttpSendRequest failed (%d)"), GetLastError());
        return false;
    }

    // Wait for asynch callback to close, or timeout (to check cancel/termination)

    while (true)
    {
        DWORD result = WaitForSingleObject(m_closedEvent, 100);

        if (result == WAIT_TIMEOUT)
        {
            if (cancel)
            {
                hRequest.WinHttpCloseHandle();
                WaitForSingleObject(m_closedEvent, INFINITE);
                CloseHandle(m_closedEvent);
                m_closedEvent = NULL;
                return false;
            }
        }
        else if (result != WAIT_OBJECT_0)
        {
            // internal error
            hRequest.WinHttpCloseHandle();
            WaitForSingleObject(m_closedEvent, INFINITE);
            CloseHandle(m_closedEvent);
            m_closedEvent = NULL;
            return false;
        }
        else
        {
            // callback has closed
            break;
        }
    }

    CloseHandle(m_closedEvent);
    m_closedEvent = NULL;

    if (m_requestSuccess)
    {
        response = m_response;
        return true;
    }

    return false;
}

void HTTPSRequest::AppendResponse(const string& responseData)
{
    AutoMUTEX lock(m_mutex);

    m_response.append(responseData);
}

bool HTTPSRequest::ValidateServerCert(PCCERT_CONTEXT pCert)
{
    AutoMUTEX lock(m_mutex);

    // Set an empty certificate when validation isn't required

    if (m_expectedServerCertificate.length() == 0)
    {
        return true;
    }

    BYTE* pbBinary = NULL; //base64 decoded pem
    DWORD cbBinary; //base64 decoded pem size
    bool bResult = false;

    //Base64 decode pem string to BYTE*
    
    //Get the expected pbBinary length
    if (!CryptStringToBinaryA(
            (LPCSTR)m_expectedServerCertificate.c_str(), m_expectedServerCertificate.length(),
            CRYPT_STRING_BASE64, NULL, &cbBinary, NULL, NULL))
    {
        my_print(false, _T("HTTPSRequest::ValidateServerCert:%d - CryptStringToBinaryA failed (%d)"), __LINE__, GetLastError());
        return false;
    }

    pbBinary = new (std::nothrow) BYTE[cbBinary];
    if (!pbBinary)
    {
        my_print(false, _T("ValidateServerCert: memory allocation failed"));
        return false;
    }

    //Perform base64 decode
    if (!CryptStringToBinaryA(
        (LPCSTR)m_expectedServerCertificate.c_str(), m_expectedServerCertificate.length(),
        CRYPT_STRING_BASE64, pbBinary, &cbBinary, NULL, NULL))
    {
        my_print(false, _T("HTTPSRequest::ValidateServerCert:%d - CryptStringToBinaryA failed (%d)"), __LINE__, GetLastError());
        return false;
    }
    
    // Check if the certificate in pCert matches the expectedServerCertificate
    if (pCert->cbCertEncoded == cbBinary)
    {
        bResult = true;

        for (unsigned int i = 0; i < cbBinary; ++i)
        {
            if (pCert->pbCertEncoded[i] != pbBinary[i])
            {
                bResult = false;
                break;
            }
        }
    }
    
    delete pbBinary;

    return bResult;
}

tstring HTTPSRequest::GetSystemDefaultHTTPSProxy()
{
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig;
    if (!WinHttpGetIEProxyConfigForCurrentUser(&proxyConfig))
    {
        return _T("");
    }

    // Proxy settings look something like this:
    // http=127.0.0.1:8081;https=127.0.0.1:8082;ftp=127.0.0.1:8083;socks=127.0.0.1:8084

    tstringstream stream(proxyConfig.lpszProxy);

    if (proxyConfig.lpszProxy) GlobalFree(proxyConfig.lpszProxy);
    if (proxyConfig.lpszProxyBypass) GlobalFree(proxyConfig.lpszProxyBypass);
    if (proxyConfig.lpszAutoConfigUrl) GlobalFree(proxyConfig.lpszAutoConfigUrl);

    tstring proxy_setting;
    while (std::getline(stream, proxy_setting, _T(';')))
    {
        size_t proxy_type_end = proxy_setting.find(_T('='));
        if (proxy_type_end > 0 && proxy_type_end < proxy_setting.length())
        {
            // Convert to lowercase.
            std::transform(
                proxy_setting.begin(), 
                proxy_setting.begin() + proxy_type_end, 
                proxy_setting.begin(), 
                ::tolower);

            if (proxy_setting.compare(0, proxy_type_end, _T("https")) == 0)
            {
                return proxy_setting.substr(proxy_type_end+1);
            }
        }
    }

    return _T("");
}
