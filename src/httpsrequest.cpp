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
#include "embeddedvalues.h"

// TODO: how to export restrictions impact general availability of crypto services?
// http://technet.microsoft.com/en-us/library/cc962093.aspx
// (Also a concern for VPN services)

#pragma comment (lib, "crypt32.lib")


HTTPSRequest::HTTPSRequest(void)
{
    m_mutex = CreateMutex(NULL, FALSE, 0);

    // Must use a manual event: multiple things wait on the same event
    m_closedEvent = CreateEvent(NULL, TRUE, FALSE, 0);
}

HTTPSRequest::~HTTPSRequest(void)
{
    // In case object is destroyed while callback is outstanding, wait
    WaitForSingleObject(m_closedEvent, INFINITE);

    CloseHandle(m_mutex);
}

void CALLBACK WinHttpStatusCallback(
                HINTERNET hRequest,
                DWORD_PTR dwContext,
                DWORD dwInternetStatus,
                LPVOID lpvStatusInformation,
                DWORD dwStatusInformationLength)
{
    HTTPSRequest* httpRequest = (HTTPSRequest*)dwContext;
    CERT_CONTEXT *pCert = {0};
    DWORD dwStatusCode;
    DWORD dwLen;
    LPVOID pBuffer = NULL;

    my_print(true, _T("HTTPS request... (%x)"), dwInternetStatus);

    switch (dwInternetStatus)
    {
    case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
        // This is ALWAYS the last notification; once it sets closed signal
        // it's safe to deallocate the parent httpRequest
        httpRequest->SetClosedEvent();
        break;
    case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:

        // TODO: is this really the earliest we can inject out custom server cert validation?
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
    case WINHTTP_CALLBACK_FLAG_REQUEST_ERROR:
    case WINHTTP_CALLBACK_FLAG_SECURE_FAILURE:
        my_print(false, _T("WinHttpStatusCallback request failed (%x)"), dwInternetStatus);
        WinHttpCloseHandle(hRequest);
        break;
    default:
        // TODO: handle all events sent for notification mask
        break;
    }
}

bool HTTPSRequest::GetRequest(
        const bool& cancel,
        const TCHAR* serverAddress,
        int serverWebPort,
        const string& webServerCertificate,
        const TCHAR* requestPath,
        string& response)
{
    DWORD dwFlags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
				    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
				    SECURITY_FLAG_IGNORE_UNKNOWN_CA;

    AutoHINTERNET hSession =
                WinHttpOpen(
                    _T("Mozilla/4.0 (compatible; MSIE 5.22)"),
	                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
	                WINHTTP_NO_PROXY_NAME,
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

    AutoHINTERNET hRequest =
            WinHttpOpenRequest(
                    hConnect,
	                _T("GET"),
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

    DWORD notificationFlags =
            WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER |
            WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER |
            WINHTTP_CALLBACK_STATUS_SENDING_REQUEST |
            WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE |
            WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE |
            WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE |
            WINHTTP_CALLBACK_FLAG_REQUEST_ERROR |
            WINHTTP_CALLBACK_FLAG_SECURE_FAILURE |
            WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING;

    if (WINHTTP_INVALID_STATUS_CALLBACK == WinHttpSetStatusCallback(
                                                hRequest,
                                                WinHttpStatusCallback,
                                                // TODO: use notificationFlags?
                                                WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS,
                                                NULL))
    {
	    my_print(false, _T("WinHttpSetStatusCallback failed (%d)"), GetLastError());
        return false;
    }

    // Kick off the asynchronous processing

    ResetEvent(m_closedEvent);
    m_expectedServerCertificate = webServerCertificate;
    m_requestSuccess = false;
    m_response = "";

    if (FALSE == WinHttpSendRequest(
                    hRequest,
	                WINHTTP_NO_ADDITIONAL_HEADERS,
	                0,
	                WINHTTP_NO_REQUEST_DATA,
	                0,
	                0,
	                (DWORD_PTR)this))
    {
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
                WinHttpCloseHandle(hRequest);
                WaitForSingleObject(m_closedEvent, INFINITE);
                return false;
            }
        }
        else if (result != WAIT_OBJECT_0)
        {
            // internal error
            WinHttpCloseHandle(hRequest);
            WaitForSingleObject(m_closedEvent, INFINITE);
            return false;
        }
        else
        {
            // callback has closed
            break;
        }
    }

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

    BYTE* pbBinary = NULL; //base64 decoded pem
    DWORD cbBinary; //base64 decoded pem size
    bool bResult = false;

    //Base64 decode pem string to BYTE*
    
    //Get the expected pbBinary length
    CryptStringToBinaryA(
        (LPCSTR)m_expectedServerCertificate.c_str(), m_expectedServerCertificate.length(),
        CRYPT_STRING_BASE64, NULL, &cbBinary, NULL, NULL);

    pbBinary = new (std::nothrow) BYTE[cbBinary];
    if (!pbBinary)
    {
        my_print(false, _T("ValidateServerCert: memory allocation failed"));
        return false;
    }

    //Perform base64 decode
    CryptStringToBinaryA(
        (LPCSTR)m_expectedServerCertificate.c_str(), m_expectedServerCertificate.length(),
        CRYPT_STRING_BASE64, pbBinary, &cbBinary, NULL, NULL);
    
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
