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
#include "logging.h"
#include "psiclient.h"
#include <WinCrypt.h>
#include "config.h"
#include "embeddedvalues.h"
#include "stopsignal.h"
#include "systemproxysettings.h"
#include "transport_connection.h"
#include "transport_registry.h"
#include "coretransport.h"
#include "utilities.h"


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


HTTPSRequest::HTTPSRequest(bool silentMode/*=false*/)
    : m_silentMode(silentMode), m_closedEvent(NULL)
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
        my_print(NOT_SENSITIVE, true, _T("%s: received no context; thread exiting"), __TFUNCTION__);
        return;
    }

    HTTPSRequest* httpRequest = (HTTPSRequest*)dwContext;
    DWORD dwStatusCode;
    DWORD dwLen;
    LPVOID pBuffer = NULL;

    //my_print(NOT_SENSITIVE, true, _T("HTTPS request... (%d)"), dwInternetStatus);

    DWORD errorCode = 0;
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
        if (httpRequest->m_expectedServerCertificate.length() > 0)
        {
            // Validate server certificate (before requesting)

            CERT_CONTEXT* pCert = NULL;
            dwLen = sizeof(pCert);
            if (!WinHttpQueryOption(
                        hRequest,
                        WINHTTP_OPTION_SERVER_CERT_CONTEXT,
                        &pCert,
                        &dwLen)
                || NULL == pCert)
            {
                my_print(NOT_SENSITIVE, httpRequest->m_silentMode, _T("WinHttpQueryOption failed (%d)"), GetLastError());
                WinHttpCloseHandle(hRequest);
                return;
            }

            bool valid = httpRequest->ValidateServerCert((PCCERT_CONTEXT)pCert);
            CertFreeCertificateContext(pCert);

            if (!valid)
            {
                my_print(NOT_SENSITIVE, httpRequest->m_silentMode, _T("ValidateServerCert failed"));
                // Close request handle immediately to prevent sending of data
                WinHttpCloseHandle(hRequest);
                return;
            }
        }
        break;
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:

        // Start downloding response

        if (!WinHttpReceiveResponse(hRequest, NULL))
        {
            my_print(NOT_SENSITIVE, httpRequest->m_silentMode, _T("WinHttpReceiveResponse failed (%d)"), GetLastError());
            WinHttpCloseHandle(hRequest);
            return;
        }
        break;
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:

        // Get Date header. We're not going to error if we can't get it.
        // This comes before the status code check, because the Date header
        // should be valid for all responses.
        dwLen = sizeof(dwStatusCode);
        if (!WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_DATE,
            WINHTTP_HEADER_NAME_BY_INDEX,
            NULL,
            &dwLen,
            WINHTTP_NO_HEADER_INDEX))
        {
            // False return is expected, because we're obtaining the required size.
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                wstring buf;
                buf.resize(dwLen / sizeof(WCHAR) + 1, '\0');

                if (WinHttpQueryHeaders(
                    hRequest,
                    WINHTTP_QUERY_DATE,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    (LPVOID)buf.data(),
                    &dwLen,
                    WINHTTP_NO_HEADER_INDEX))
                {
                    httpRequest->ResponseSetDateHeader(WStringToUTF8(buf));
                }
            }
        }

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
            my_print(NOT_SENSITIVE, httpRequest->m_silentMode, _T("WinHttpQueryHeaders failed (%d)"), GetLastError());
            WinHttpCloseHandle(hRequest);
            return;
        }

        httpRequest->ResponseSetCode(dwStatusCode);
        my_print(NOT_SENSITIVE, true, _T("HTTP request status code: %d"), dwStatusCode);

        if (!WinHttpQueryDataAvailable(hRequest, 0))
        {
            my_print(NOT_SENSITIVE, httpRequest->m_silentMode, _T("WinHttpQueryDataAvailable failed (%d)"), GetLastError());
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
            my_print(NOT_SENSITIVE, httpRequest->m_silentMode, _T("WinHttpReadData failed (%d)"), GetLastError());
            HeapFree(GetProcessHeap(), 0, pBuffer);
            WinHttpCloseHandle(hRequest);
            return;
        }

        // NOTE: response data may be binary; some relevant comments here...
        // http://stackoverflow.com/questions/441203/proper-way-to-store-binary-data-with-c-stl

        httpRequest->ResponseAppendBody(string(string::const_pointer(pBuffer), string::const_pointer((char*)pBuffer + dwLen)));

        HeapFree(GetProcessHeap(), 0, pBuffer);

        // Check for more data

        if (!WinHttpQueryDataAvailable(hRequest, 0))
        {
            my_print(NOT_SENSITIVE, httpRequest->m_silentMode, _T("WinHttpQueryDataAvailable failed (%d)"), GetLastError());
            WinHttpCloseHandle(hRequest);
            return;
        }
        break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
        // Get error value as per http://msdn.microsoft.com/en-us/library/aa383917%28v=VS.85%29.aspx
        if (ERROR_WINHTTP_OPERATION_CANCELLED != ((WINHTTP_ASYNC_RESULT*)lpvStatusInformation)->dwError)
        {
            my_print(NOT_SENSITIVE,
                     httpRequest->m_silentMode, _T("HTTP request error (%d, %d)"),
                     ((WINHTTP_ASYNC_RESULT*)lpvStatusInformation)->dwResult,
                     ((WINHTTP_ASYNC_RESULT*)lpvStatusInformation)->dwError);
        }
        WinHttpCloseHandle(hRequest);
        break;
    case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
        if (lpvStatusInformation)
        {
            errorCode = *(DWORD *)lpvStatusInformation;
        }
        my_print(NOT_SENSITIVE, httpRequest->m_silentMode, _T("HTTP secure failure (%d)"), errorCode);
        WinHttpCloseHandle(hRequest);
        break;
    default:
        // No action on other events
        break;
    }
}

bool HTTPSRequest::MakeRequest(
        const TCHAR* serverAddress,
        int serverWebPort,
        const string& webServerCertificate,
        const TCHAR* requestPath,
        const StopInfo& stopInfo,
        PsiphonProxy usePsiphonLocalProxy,
        HTTPSRequest::Response& response,
        bool failoverToURLProxy/*=false*/,
        LPCWSTR additionalHeaders/*=NULL*/,
        LPVOID additionalData/*=NULL*/,
        DWORD additionalDataLength/*=0*/,
        LPCWSTR httpVerb/*=NULL*/)
{
    // The WinHTTP client appears to be no longer capable of making HTTPS requests to Amazon S3:
    // http://stackoverflow.com/questions/29801450/winhttp-doesnt-download-from-amazon-s3-on-winxp
    // In this case, we can use the tunnel core URL proxy to make the request using a different http client stack.

    bool success = MakeRequestWithURLProxyOption(
        serverAddress, serverWebPort, webServerCertificate, requestPath,
        stopInfo, usePsiphonLocalProxy, response,
        false, // useURLProxy
        additionalHeaders, additionalData, additionalDataLength, httpVerb);

    my_print(NOT_SENSITIVE, true, _T("%s:%d - MakeRequestWithURLProxyOption %s : %d"), __TFUNCTION__, __LINE__, (success ? _T("succeeded") : _T("failed")), GetLastError());

    if (!success && failoverToURLProxy)
    {
        my_print(NOT_SENSITIVE, true, _T("%s:%d - failing over to URL proxy"), __TFUNCTION__, __LINE__);

        // This is the broken SSL WinHTTP client case.
        // Use a URL proxy to make the HTTPS request for us instead.

        // This is the format of requests to the URL proxy
        // http://localhost:URL_proxy_port/<direct|tunneled>/urlencode(https://destination:port/requestPath)
        tstringstream URL;
        URL << _T("https://") << serverAddress << _T(":") << serverWebPort << requestPath;

        auto tunneledProxyConfig = GetTunneledDefaultProxyConfig();

        if (tunneledProxyConfig.HTTPEnabled())
        {
            // We already have a tunnel, so we'll use that for the URL proxy

            tstringstream urlProxyRequestPath;
            if (usePsiphonLocalProxy == PsiphonProxy::DONT_USE) 
            {
                urlProxyRequestPath << _T("/direct/");
                my_print(NOT_SENSITIVE, true, _T("%s:%d - Making direct URL proxy request with existing tunnel-core"), __TFUNCTION__, __LINE__);
            }
            else // USE or REQUIRE
            {
                urlProxyRequestPath << _T("/tunneled/");
                my_print(NOT_SENSITIVE, true, _T("%s:%d - Making tunneled URL proxy request with existing tunnel-core"), __TFUNCTION__, __LINE__);
            }
            urlProxyRequestPath << UrlEncode(URL.str());

            success = MakeRequestWithURLProxyOption(
                tunneledProxyConfig.HTTPHostname().c_str(), tunneledProxyConfig.HTTPPort(), 
                webServerCertificate, urlProxyRequestPath.str().c_str(),
                stopInfo, usePsiphonLocalProxy, response,
                true, // useURLProxy
                additionalHeaders, additionalData, additionalDataLength, httpVerb);
        }
        else if (usePsiphonLocalProxy == PsiphonProxy::REQUIRE)
        {
            // We don't have a tunnel, but we must make the request tunneled, so we can't proceed
            success = false;
        }
        else
        {
            // We don't have a tunnel, so we'll create an unconnected tunnel-core
            // instance to use as the direct URL proxy.

            try
            {
                TransportConnection connection;

                // Throws on failure
                connection.Connect(
                    stopInfo,
                    TransportRegistry::New(CORE_TRANSPORT_PROTOCOL_NAME),
                    NULL, // not receiving reconnection notifications
                    NULL, // not receiving upgrade paver calls
                    NULL, // not collecting stats
                    NULL, // not supplying authorizations
                    new ServerEntry(), // this empty ServerEntry is the flag for URL proxy mode; we don't need to connect to a specific server
                    true);// don't apply system proxy settings (or write to the Psiphon proxy settings registry key)
                          // as another transport might currently be running

                // NOTE that we will always make "direct" requests since this URL proxy will not establish a tunnel
                tstringstream urlProxyRequestPath;
                urlProxyRequestPath << _T("/") << _T("direct") << _T("/") << UrlEncode(URL.str());

                my_print(NOT_SENSITIVE, true, _T("%s:%d - Making direct URL proxy request with temp tunnel-core"), __TFUNCTION__, __LINE__);

                success = MakeRequestWithURLProxyOption(
                    _T("127.0.0.1"), connection.GetTransportLocalHttpProxy(), 
                    webServerCertificate, urlProxyRequestPath.str().c_str(),
                    stopInfo, usePsiphonLocalProxy, response,
                    true, // useURLProxy
                    additionalHeaders, additionalData, additionalDataLength, httpVerb);

                // Note that when we leave this scope, the TransportConnection will
                // clean up the transport connection.
            }
            catch (StopSignal::StopException&)
            {
                throw;
            }
            catch (...)
            {
                my_print(NOT_SENSITIVE, true, _T("Failed to start URL proxy"), __TFUNCTION__);
                success = false;
            }
        }
    }

    return success;
}

// Throws StopSignal::StopException if stop was signaled.
bool HTTPSRequest::MakeRequestWithURLProxyOption(
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
        LPCWSTR httpVerb)
{
    // Throws if signaled
    stopInfo.stopSignal->CheckSignal(stopInfo.stopReasons, true);

    DWORD dwFlags = 0;

    if (webServerCertificate.length() > 0)
    {
        // We're doing our own validation, so don't choke on cert errors.
        dwFlags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                    SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    }

    tstring proxyHost;
    if (useURLProxy)
    {
        // The URL proxy (tunnelcore) will pick up a system proxy setting and will use it if necessary.
        // So leave proxyHost blank here.
    }
    else if (usePsiphonLocalProxy != PsiphonProxy::DONT_USE)
    {
        auto tunneledProxyConfig = GetTunneledDefaultProxyConfig();

        if (usePsiphonLocalProxy == PsiphonProxy::REQUIRE && !tunneledProxyConfig.HTTPEnabled())
        {
            // We are required to proxy through Psiphon, but there's no Psiphon
            // proxy to use.
            return false;
        }
    }
    else
    {
        proxyHost = GetNativeDefaultProxyConfig().HTTPHostPort();
    }

    tstring reqType(requestPath);
    size_t reqEnd = reqType.find(_T('?'));
    if (reqEnd != string::npos)
    {
        reqType.resize(reqEnd);
    }
    my_print(NOT_SENSITIVE, true, _T("%s: %s; proxy: {use: %d, set: %d}"), __TFUNCTION__, reqType.c_str(), usePsiphonLocalProxy, !!proxyHost.length());

    AutoHINTERNET hSession =
                WinHttpOpen(
                    _T("Mozilla/4.0 (compatible; MSIE 5.22)"),
                    proxyHost.length() ? WINHTTP_ACCESS_TYPE_NAMED_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    proxyHost.length() ? proxyHost.c_str() : WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS,
                    WINHTTP_FLAG_ASYNC);

    if (NULL == hSession)
    {
        my_print(NOT_SENSITIVE, m_silentMode, _T("WinHttpOpen failed (%d)"), GetLastError());
        return false;
    }

    if (FALSE == WinHttpSetTimeouts(hSession, 0, HTTPS_REQUEST_CONNECT_TIMEOUT_MS,
                            HTTPS_REQUEST_SEND_TIMEOUT_MS, HTTPS_REQUEST_RECEIVE_TIMEOUT_MS))
    {
        my_print(NOT_SENSITIVE, m_silentMode, _T("WinHttpSetTimeouts failed (%d)"), GetLastError());
        return false;
    }

    // SSLv3, TLSv1.0, TLSv1.1 all have security flaws that mean that should be avoided. 
    // Some of those flaws (like SSLv3's POODLE http://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2014-3566)
    // require the client side to not try to use them. So we're going to force use of 
    // TLS v1.2. We'll try to remember to update these flags when new TLS versions come
    // out; we think it's too risky to set all bits except the bad ones (like ~(SSL|TLS1.0|TLS1.1)), 
    // as we might get something we don't want.
    // When WinHttpSetOption gets flags it doesn't understand -- like WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
    // on XP and Vista -- it returns FALSE and sets errno to ERROR_INVALID_PARAMETER (87). When 
    // that happens we'll fall back to the URL proxy. That's why we're _not_ going
    // to set the HTTPS protocol for URL proxy requests (and it's HTTP, not HTTPS).
    DWORD dwProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    if (!useURLProxy)
    {
        if (FALSE == WinHttpSetOption(
            hSession,
            WINHTTP_OPTION_SECURE_PROTOCOLS,
            &dwProtocols,
            sizeof(DWORD)))
        {
            my_print(NOT_SENSITIVE, m_silentMode, _T("WinHttpSetOption WINHTTP_OPTION_SECURE_PROTOCOLS failed (%d)"), GetLastError());
            return false;
        }
    }

    AutoHINTERNET hConnect =
            WinHttpConnect(
                hSession,
                serverAddress,
                (INTERNET_PORT)serverWebPort,
                0);

    if (NULL == hConnect)
    {
        my_print(NOT_SENSITIVE, m_silentMode, _T("WinHttpConnect failed (%d)"), GetLastError());
        return false;
    }

    if (!httpVerb)
    {
        httpVerb = additionalData ? _T("POST") : _T("GET");
    }

    AutoHINTERNET hRequest =
            WinHttpOpenRequest(
                    hConnect,
                    httpVerb,
                    requestPath,
                    NULL,
                    WINHTTP_NO_REFERER,
                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                    useURLProxy ? 0 : WINHTTP_FLAG_SECURE); // disable SSL for URL proxy requests

    if (NULL == hRequest)
    {
        my_print(NOT_SENSITIVE, m_silentMode, _T("WinHttpOpenRequest failed (%d)"), GetLastError());
        return false;
    }

    if (FALSE == WinHttpSetOption(
                    hRequest,
                    WINHTTP_OPTION_SECURITY_FLAGS,
                    &dwFlags,
                    sizeof(DWORD)))
    {
        my_print(NOT_SENSITIVE, m_silentMode, _T("WinHttpSetOption WINHTTP_OPTION_SECURITY_FLAGS failed (%d)"), GetLastError());
        return false;
    }

    if (WINHTTP_INVALID_STATUS_CALLBACK == WinHttpSetStatusCallback(
                                                hRequest,
                                                WinHttpStatusCallback,
                                                WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS,
                                                NULL))
    {
        my_print(NOT_SENSITIVE, m_silentMode, _T("WinHttpSetStatusCallback failed (%d)"), GetLastError());
        return false;
    }

    // Kick off the asynchronous processing

    // Must use a manual event: multiple things wait on the same event
    assert(m_closedEvent == NULL);
    m_closedEvent = CreateEvent(NULL, TRUE, FALSE, 0);

    if (m_closedEvent == NULL)
    {
        stringstream error;
        error << __FUNCTION__ << ":" << __LINE__ << ": CreateEvent failed. Out of memory";
        throw std::exception(error.str().c_str());
    }

    // Tunnel-core drop idle connections after 30 seconds. If this is faster
    // than the server indicated to the client on the last request, we can get
    // an error that won't be auto-retried by WinHTTP.
    // For example, PsiCash's ELB idle connection timeout was 60 seconds. So
    // any repeat PsiCash request made between 30 and 60 seconds of the
    // previous one would result in a hard error (not retried anywhere).
    // We're going to specify Connection:close in all our requests to avoid
    // this problem.
    wstring headers = L"Connection: close\r\n";
    if (additionalHeaders)
    {
        headers += additionalHeaders;
    }


    m_expectedServerCertificate = webServerCertificate;
    m_requestSuccess = false;
    m_response = Response();

    if (FALSE == WinHttpSendRequest(
                    hRequest,
                    headers.c_str(),
                    headers.length(),
                    additionalData ? additionalData : WINHTTP_NO_REQUEST_DATA,
                    additionalDataLength,
                    additionalDataLength,
                    (DWORD_PTR)this))
    {
        CloseHandle(m_closedEvent);
        m_closedEvent = NULL;
        my_print(NOT_SENSITIVE, m_silentMode, _T("WinHttpSendRequest failed (%d)"), GetLastError());
        return false;
    }

    // Wait for asynch callback to close, or timeout (to check cancel/termination)

    while (true)
    {
        DWORD result = WaitForSingleObject(m_closedEvent, 100);

        if (result == WAIT_TIMEOUT)
        {
            if (stopInfo.stopSignal->CheckSignal(stopInfo.stopReasons, false))
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
        response = std::move(m_response);
        m_response = Response();
        return true;
    }

    return false;
}

void HTTPSRequest::ResponseAppendBody(const string& responseData)
{
    AutoMUTEX lock(m_mutex);
    m_response.body.append(responseData);
}

void HTTPSRequest::ResponseSetCode(int code)
{
    AutoMUTEX lock(m_mutex);
    m_response.code = code;
}

void HTTPSRequest::ResponseSetDateHeader(const string& dateHeader)
{
    AutoMUTEX lock(m_mutex);
    m_response.dateHeader = dateHeader;
}

bool HTTPSRequest::ValidateServerCert(PCCERT_CONTEXT pCert)
{
    AutoMUTEX lock(m_mutex);

    // Set an empty certificate when validation isn't required

    if (m_expectedServerCertificate.length() == 0)
    {
        // We shouldn't be here if there's no cert to check against.
        assert(0);
        return false;
    }

    BYTE* pbBinary = NULL; //base64 decoded pem
    DWORD cbBinary = 0; //base64 decoded pem size
    bool bResult = false;

    //Base64 decode pem string to BYTE*

    //Get the expected pbBinary length
    if (!CryptStringToBinaryA(
            (LPCSTR)m_expectedServerCertificate.c_str(), m_expectedServerCertificate.length(),
            CRYPT_STRING_BASE64, NULL, &cbBinary, NULL, NULL))
    {
        my_print(NOT_SENSITIVE, m_silentMode, _T("HTTPSRequest::ValidateServerCert:%d - CryptStringToBinaryA failed (%d)"), __LINE__, GetLastError());
        return false;
    }

    pbBinary = new (std::nothrow) BYTE[cbBinary];
    if (!pbBinary)
    {
        my_print(NOT_SENSITIVE, m_silentMode, _T("ValidateServerCert: memory allocation failed"));
        return false;
    }

    //Perform base64 decode
    if (!CryptStringToBinaryA(
        (LPCSTR)m_expectedServerCertificate.c_str(), m_expectedServerCertificate.length(),
        CRYPT_STRING_BASE64, pbBinary, &cbBinary, NULL, NULL))
    {
        my_print(NOT_SENSITIVE, m_silentMode, _T("HTTPSRequest::ValidateServerCert:%d - CryptStringToBinaryA failed (%d)"), __LINE__, GetLastError());
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
