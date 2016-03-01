/*
 * Copyright (c) 2013, Psiphon Inc.
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
#include "logging.h"
#include "psiclient.h"
#include "embeddedvalues.h"
#include "httpsrequest.h"
#include "wininet_network_check.h"
#include "systemproxysettings.h"
#include "utilities.h"
#include "diagnostic_info.h"
#include "osrng.h"
#include "usersettings.h"


HANDLE g_diagnosticHistoryMutex = CreateMutex(NULL, FALSE, 0);
Json::Value g_diagnosticHistory(Json::arrayValue);


// This is really just a non-template wrapper around AddDiagnosticInfo, to help
// users of it recognize that they can pass a Json::Value.
void AddDiagnosticInfoJson(const char* message, const Json::Value& jsonValue)
{
    AddDiagnosticInfo(message, jsonValue);
}

void AddDiagnosticInfoJson(const char* message, const char* jsonString)
{
    if (!jsonString) {
        AddDiagnosticInfo(message, Json::nullValue);

    }

    Json::Value json;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(jsonString, json);
    if (!parsingSuccessful)
    {
        return;
    }

    AddDiagnosticInfo(message, json);
}

void GetDiagnosticHistory(Json::Value& o_json)
{
    o_json.clear();
    AutoMUTEX mutex(g_diagnosticHistoryMutex);
    o_json = Json::Value(g_diagnosticHistory);
}



// Adapted from http://www.codeproject.com/Articles/66016/A-Quick-Start-Guide-of-Process-Mandatory-Level-Che
// Original comments:
/*
* Copyright (c) Microsoft Corporation.
* 
* User Account Control (UAC) is a new security component in Windows Vista and 
* newer operating systems. With UAC fully enabled, interactive administrators 
* normally run with least user privileges. This example demonstrates how to 
* check the privilege level of the current process, and how to self-elevate 
* the process by giving explicit consent with the Consent UI. 
* 
* This source is subject to the Microsoft Public License.
* See http://www.microsoft.com/opensource/licenses.mspx#Ms-PL.
* All other rights reserved.
* 
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/
//
//   FUNCTION: IsUserInAdminGroup()
//
//   PURPOSE: The function checks whether the primary access token of the 
//   process belongs to user account that is a member of the local 
//   Administrators group, even if it currently is not elevated.
//
//   RETURN VALUE: Returns TRUE if the primary access token of the process 
//   belongs to user account that is a member of the local Administrators 
//   group. Returns FALSE if the token does not.
//
//   EXCEPTION: If this function fails, it throws a C++ DWORD exception which 
//   contains the Win32 error code of the failure.
//
//   EXAMPLE CALL:
//     try 
//     {
//         if (IsUserInAdminGroup())
//             wprintf (L"User is a member of the Administrators group\n");
//         else
//             wprintf (L"User is not a member of the Administrators group\n");
//     }
//     catch (DWORD dwError)
//     {
//         wprintf(L"IsUserInAdminGroup failed w/err %lu\n", dwError);
//     }
//
struct UserGroupInfo {
    bool inAdminsGroup;
    bool inUsersGroup;
    bool inGuestsGroup;
    bool inPowerUsersGroup;
};

bool GetUserGroupInfo(UserGroupInfo& groupInfo)
{
    BOOL fInGroup = FALSE;
    DWORD dwError = ERROR_SUCCESS;
    HANDLE hToken = NULL;
    HANDLE hTokenToCheck = NULL;
    DWORD cbSize = 0;
    OSVERSIONINFO osver = { sizeof(osver) };

    // Open the primary access token of the process for query and duplicate.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, 
        &hToken))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    // Determine whether system is running Windows Vista or later operating 
    // systems (major version >= 6) because they support linked tokens, but 
    // previous versions (major version < 6) do not.
    if (!GetVersionEx(&osver))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    if (osver.dwMajorVersion >= 6)
    {
        // Running Windows Vista or later (major version >= 6). 
        // Determine token type: limited, elevated, or default. 
        TOKEN_ELEVATION_TYPE elevType;
        if (!GetTokenInformation(hToken, TokenElevationType, &elevType, 
            sizeof(elevType), &cbSize))
        {
            dwError = GetLastError();
            goto Cleanup;
        }

        // If limited, get the linked elevated token for further check.
        if (TokenElevationTypeLimited == elevType)
        {
            if (!GetTokenInformation(hToken, TokenLinkedToken, &hTokenToCheck, 
                sizeof(hTokenToCheck), &cbSize))
            {
                dwError = GetLastError();
                goto Cleanup;
            }
        }
    }
    
    // CheckTokenMembership requires an impersonation token. If we just got a 
    // linked token, it already is an impersonation token.  If we did not get 
    // a linked token, duplicate the original into an impersonation token for 
    // CheckTokenMembership.
    if (!hTokenToCheck)
    {
        if (!DuplicateToken(hToken, SecurityIdentification, &hTokenToCheck))
        {
            dwError = GetLastError();
            goto Cleanup;
        }
    }

    // Create the SID corresponding to the Administrators group.
    BYTE groupSID[SECURITY_MAX_SID_SIZE];
    
    //
    // ADMINS
    //

    cbSize = sizeof(groupSID);
    if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, &groupSID,  
        &cbSize))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    // Check if the token to be checked contains admin SID.
    // http://msdn.microsoft.com/en-us/library/aa379596(VS.85).aspx:
    // To determine whether a SID is enabled in a token, that is, whether it 
    // has the SE_GROUP_ENABLED attribute, call CheckTokenMembership.
    if (!CheckTokenMembership(hTokenToCheck, &groupSID, &fInGroup)) 
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    groupInfo.inAdminsGroup = !!fInGroup;

    //
    // USERS
    //

    cbSize = sizeof(groupSID);
    if (!CreateWellKnownSid(WinBuiltinUsersSid, NULL, &groupSID,  
        &cbSize))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    // Check if the token to be checked contains admin SID.
    // http://msdn.microsoft.com/en-us/library/aa379596(VS.85).aspx:
    // To determine whether a SID is enabled in a token, that is, whether it 
    // has the SE_GROUP_ENABLED attribute, call CheckTokenMembership.
    if (!CheckTokenMembership(hTokenToCheck, &groupSID, &fInGroup)) 
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    groupInfo.inUsersGroup = !!fInGroup;

    //
    // GUESTS
    //

    cbSize = sizeof(groupSID);
    if (!CreateWellKnownSid(WinBuiltinGuestsSid, NULL, &groupSID,  
        &cbSize))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    // Check if the token to be checked contains admin SID.
    // http://msdn.microsoft.com/en-us/library/aa379596(VS.85).aspx:
    // To determine whether a SID is enabled in a token, that is, whether it 
    // has the SE_GROUP_ENABLED attribute, call CheckTokenMembership.
    if (!CheckTokenMembership(hTokenToCheck, &groupSID, &fInGroup)) 
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    groupInfo.inGuestsGroup = !!fInGroup;

    //
    // POWER USERS
    //

    cbSize = sizeof(groupSID);
    if (!CreateWellKnownSid(WinBuiltinPowerUsersSid, NULL, &groupSID,  
        &cbSize))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    // Check if the token to be checked contains admin SID.
    // http://msdn.microsoft.com/en-us/library/aa379596(VS.85).aspx:
    // To determine whether a SID is enabled in a token, that is, whether it 
    // has the SE_GROUP_ENABLED attribute, call CheckTokenMembership.
    if (!CheckTokenMembership(hTokenToCheck, &groupSID, &fInGroup)) 
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    groupInfo.inPowerUsersGroup = !!fInGroup;

Cleanup:
    // Centralized cleanup for all allocated resources.
    if (hToken)
    {
        CloseHandle(hToken);
        hToken = NULL;
    }
    if (hTokenToCheck)
    {
        CloseHandle(hTokenToCheck);
        hTokenToCheck = NULL;
    }

    // Throw the error if something failed in the function.
    if (ERROR_SUCCESS != dwError)
    {
        return false;
    }

    return true;
}


struct SystemInfo
{
    wstring name;
    wstring version;
    wstring codeSet;
    wstring countryCode;
    UINT64 freePhysicalMemoryKB;
    UINT64 freeVirtualMemoryKB;
    wstring locale;
    wstring architecture;
    UINT32 language;
    UINT16 servicePackMajor;
    UINT16 servicePackMinor;
    wstring status;
    wstring mshtmlDLLVersion;

    bool starter;
    bool mideastEnabled;
    bool slowMachine;
    bool wininet_success;
    WininetNetworkInfo wininet_info;
    bool userIsAdmin;
    bool groupInfo_success;
    UserGroupInfo groupInfo;
};

bool GetSystemInfo(SystemInfo& o_sysInfo)
{
    // This code adapted from: http://msdn.microsoft.com/en-us/library/aa390423.aspx

    HRESULT hr;

    hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED); 
    if (FAILED(hr)) 
    { 
        assert(0);
        return FALSE;
    }

    // Obtain the initial locator to WMI

    IWbemLocator *pLoc = NULL;

    hr = CoCreateInstance(
        CLSID_WbemLocator,             
        0, 
        CLSCTX_INPROC_SERVER, 
        IID_IWbemLocator, 
        (LPVOID *) &pLoc);
 
    if (FAILED(hr))
    {
        assert(0);
        CoUninitialize();
        return false;
    }

    // Connect to WMI through the IWbemLocator::ConnectServer method

    IWbemServices *pSvc = NULL;

    // Connect to the root\CIMV2 namespace with
    // the current user and obtain pointer pSvc
    // to make IWbemServices calls.

    BSTR wmiNamespace = SysAllocString(L"ROOT\\CIMV2");
    hr = pLoc->ConnectServer(
             wmiNamespace,               // Object path of WMI namespace
             NULL,                    // User name. NULL = current user
             NULL,                    // User password. NULL = current
             0,                       // Locale. NULL indicates current
             NULL,                    // Security flags.
             0,                       // Authority (for example, Kerberos)
             0,                       // Context object 
             &pSvc                    // pointer to IWbemServices proxy
             );
    SysFreeString(wmiNamespace);

    if (FAILED(hr))
    {
        pLoc->Release();
        CoUninitialize();
        return false;
    }

    // Set security levels on the proxy

    hr = CoSetProxyBlanket(
       pSvc,                        // Indicates the proxy to set
       RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
       RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
       NULL,                        // Server principal name 
       RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
       RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
       NULL,                        // client identity
       EOAC_NONE                    // proxy capabilities 
    );

    if (FAILED(hr))
    {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return false;
    }

    // Use the IWbemServices pointer to make requests of WMI

    BSTR queryLanguage = SysAllocString(L"WQL");
    BSTR query = SysAllocString(L"SELECT * FROM Win32_OperatingSystem");

    IEnumWbemClassObject* pEnumerator = NULL;
    hr = pSvc->ExecQuery(
        queryLanguage, 
        query,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
        NULL,
        &pEnumerator);

    SysFreeString(queryLanguage);
    SysFreeString(query);

    if (FAILED(hr))
    {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return false;
    }

    // Get the data from the query

    IWbemClassObject *pclsObj;
    ULONG uReturn = 0;

    while (pEnumerator)
    {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        // TODO: Do we need to check the value of hr? For example, will it 
        // be FAILED if the table name is bad? The sample code does not check.

        if (0 == uReturn)
        {
            break;
        }

        // For descriptions of the fields, see: 
        // http://msdn.microsoft.com/en-us/library/windows/desktop/aa394239%28v=vs.85%29.aspx

        VARIANT vtProp;

        hr = pclsObj->Get(L"Caption", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.name = vtProp.bstrVal;
            VariantClear(&vtProp);
        }

        hr = pclsObj->Get(L"Version", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.version = vtProp.bstrVal;
            VariantClear(&vtProp);
        }

        hr = pclsObj->Get(L"CodeSet", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.codeSet = vtProp.bstrVal;
            VariantClear(&vtProp);
        }

        hr = pclsObj->Get(L"CountryCode", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.countryCode = vtProp.bstrVal;
            VariantClear(&vtProp);
        }

        // The MSDN documentation says that FreePhysicalMemory and FreeVirtualMemory
        // are uint64, but in practice vtProp.ullVal is getting bad values. We'll 
        // get the string value and convert.
        hr = pclsObj->Get(L"FreePhysicalMemory", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.freePhysicalMemoryKB = std::wcstoll(vtProp.bstrVal, NULL, 10);
            VariantClear(&vtProp);
        }

        hr = pclsObj->Get(L"FreeVirtualMemory", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.freeVirtualMemoryKB = std::wcstoll(vtProp.bstrVal, NULL, 10);
            VariantClear(&vtProp);
        }

        hr = pclsObj->Get(L"Locale", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.locale = vtProp.bstrVal;
            VariantClear(&vtProp);
        }

        hr = pclsObj->Get(L"OSArchitecture", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.architecture = vtProp.bstrVal;
            VariantClear(&vtProp);
        }

        hr = pclsObj->Get(L"OSLanguage", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.language = vtProp.uintVal;
            VariantClear(&vtProp);
        }

        hr = pclsObj->Get(L"ServicePackMajorVersion", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.servicePackMajor = vtProp.uiVal;
            VariantClear(&vtProp);
        }

        hr = pclsObj->Get(L"ServicePackMinorVersion", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.servicePackMinor = vtProp.uiVal;
            VariantClear(&vtProp);
        }

        hr = pclsObj->Get(L"Status", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr))
        {
            o_sysInfo.status = vtProp.bstrVal;
            VariantClear(&vtProp);
        }

        pclsObj->Release();

        // We only want one result
        break;
    }

    pEnumerator->Release();

    // Cleanup

    pSvc->Release();
    pLoc->Release();

    CoUninitialize();

    // Miscellaneous

    o_sysInfo.starter = (GetSystemMetrics(SM_STARTER) != 0);

    o_sysInfo.mideastEnabled = (GetSystemMetrics(SM_MIDEASTENABLED) != 0);
    o_sysInfo.slowMachine = (GetSystemMetrics(SM_SLOWMACHINE) != 0);

    // Network info

    WininetNetworkInfo netInfo;
    o_sysInfo.wininet_success = false;
    if (WininetGetNetworkInfo(netInfo))
    {
        o_sysInfo.wininet_success = true;
        o_sysInfo.wininet_info = netInfo;
    }

    o_sysInfo.groupInfo_success = false;
    if (GetUserGroupInfo(o_sysInfo.groupInfo))
    {
        o_sysInfo.groupInfo_success = true;
    }

    // System DLL versions

    LPCTSTR filename = TEXT("MSHTML.DLL");
    DWORD dw;
    DWORD fileVersionInfoSize = GetFileVersionInfoSize(filename, &dw);
    if (fileVersionInfoSize > 0)
    {
        LPVOID fileVersionInfoBytes = new byte[fileVersionInfoSize];
        if (!fileVersionInfoBytes)
        {
	        throw std::exception(__FUNCTION__ ":" STRINGIZE(__LINE__) ": memory allocation failed");
        }

        auto cleanup = finally([fileVersionInfoBytes] {
            delete[] fileVersionInfoBytes;
        });

        if (GetFileVersionInfo(
            filename,
            0,
            fileVersionInfoSize,
            fileVersionInfoBytes))
        {
            struct LANGANDCODEPAGE {
                WORD wLanguage;
                WORD wCodePage;
            } *langAndCodePages;
            UINT langAndCodePagesSize = 0;

            // Read the list of languages and code pages.

            if (VerQueryValue(fileVersionInfoBytes,
                TEXT("\\VarFileInfo\\Translation"),
                (LPVOID*)&langAndCodePages,
                &langAndCodePagesSize))
            {
                TCHAR subBlock[50];
                for (size_t i = 0; i < (langAndCodePagesSize / sizeof(struct LANGANDCODEPAGE)); i++)
                {
                    if (langAndCodePages[i].wLanguage == 0x0409)
                    {
                        swprintf_s(
                            subBlock,
                            sizeof(subBlock) / sizeof(TCHAR),
                            TEXT("\\StringFileInfo\\%04x%04x\\FileVersion"),
                            langAndCodePages[i].wLanguage,
                            langAndCodePages[i].wCodePage);

                        LPCTSTR fileVersion = NULL;
                        UINT fvLen = 0;
                        if (VerQueryValue(fileVersionInfoBytes,
                            subBlock,
                            (LPVOID*)&fileVersion,
                            &fvLen))
                        {                            
                            o_sysInfo.mshtmlDLLVersion = fileVersion;
                        }
                    }
                }
            }
        }
    }
    
    return true;
}

bool GetCountryDialingCode(wstring& o_countryDialingCode)
{
    o_countryDialingCode.clear();

    SystemInfo sysInfo;
    if (!GetSystemInfo(sysInfo))
    {
        return false;
    }

    o_countryDialingCode = sysInfo.countryCode;
    return true;
}

// Not all of these fields will be used, depending on the OS version
struct SecurityInfo
{
    tstring displayName;
    tstring version; // "v1" or "v2"
    struct V1 {
        bool productUpToDate;
        bool enabled;
        tstring versionNumber;
        V1() : productUpToDate(false), enabled(false) {}
    } v1;
    struct V2 {
        DWORD productState;
        tstring securityProvider;
        bool enabled;
        bool definitionsUpToDate;
        V2() : productState(0), enabled(false), definitionsUpToDate(false) {}
    } v2;
};

void GetOSSecurityInfo(
        vector<SecurityInfo>& antiVirusInfo, 
        vector<SecurityInfo>& antiSpywareInfo, 
        vector<SecurityInfo>& firewallInfo)
{
    antiVirusInfo.clear();
    antiSpywareInfo.clear();
    firewallInfo.clear();

    // This code adapted from: http://msdn.microsoft.com/en-us/library/aa390423.aspx

    HRESULT hr;

    hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED); 
    if (FAILED(hr)) 
    { 
        assert(0);
        return;
    }

    // Obtain the initial locator to WMI

    IWbemLocator *pLoc = NULL;

    hr = CoCreateInstance(
        CLSID_WbemLocator,             
        0, 
        CLSCTX_INPROC_SERVER, 
        IID_IWbemLocator, 
        (LPVOID *) &pLoc);
 
    if (FAILED(hr))
    {
        assert(0);
        CoUninitialize();
        return;
    }

    // Connect to WMI through the IWbemLocator::ConnectServer method

    IWbemServices *pSvc = NULL;

    // Connect to the root\SecurityCenter namespace with
    // the current user and obtain pointer pSvc
    // to make IWbemServices calls.

    // NOTE: pre-Windows Vista, the namespace used was "SecurityCenter". With
    // Windows Vista, the namespace changed to "SecurityCenter2". (The former
    // still exists, but it's empty.)
    // We will try to determine which to use by first attempting to get
    // the newer namespace, and if that fails we'll get the older one.

    bool securityCenter2 = true;
    BSTR wmiNamespace = SysAllocString(L"ROOT\\SecurityCenter2");
    hr = pLoc->ConnectServer(
             wmiNamespace,               // Object path of WMI namespace
             NULL,                    // User name. NULL = current user
             NULL,                    // User password. NULL = current
             0,                       // Locale. NULL indicates current
             NULL,                    // Security flags.
             0,                       // Authority (for example, Kerberos)
             0,                       // Context object 
             &pSvc                    // pointer to IWbemServices proxy
             );
    SysFreeString(wmiNamespace);

    if (FAILED(hr))
    {
        securityCenter2 = false;

        wmiNamespace = SysAllocString(L"ROOT\\SecurityCenter");
        hr = pLoc->ConnectServer(
                 wmiNamespace,               // Object path of WMI namespace
                 NULL,                    // User name. NULL = current user
                 NULL,                    // User password. NULL = current
                 0,                       // Locale. NULL indicates current
                 NULL,                    // Security flags.
                 0,                       // Authority (for example, Kerberos)
                 0,                       // Context object 
                 &pSvc                    // pointer to IWbemServices proxy
                 );
        SysFreeString(wmiNamespace);

        if (FAILED(hr))
        {
            pLoc->Release();
            CoUninitialize();
            return;
        }
    }

    // Set security levels on the proxy

    hr = CoSetProxyBlanket(
       pSvc,                        // Indicates the proxy to set
       RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
       RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
       NULL,                        // Server principal name 
       RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
       RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
       NULL,                        // client identity
       EOAC_NONE                    // proxy capabilities 
    );

    if (FAILED(hr))
    {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // Use the IWbemServices pointer to make requests of WMI

    struct WmiLookup {
        wstring query;
        vector<SecurityInfo>& results;
        WmiLookup(wstring query, vector<SecurityInfo>& results) : query(query), results(results) {}
    };
    vector<WmiLookup> wmiLookups;
    wmiLookups.push_back(WmiLookup(L"SELECT * FROM AntiVirusProduct", antiVirusInfo));
    wmiLookups.push_back(WmiLookup(L"SELECT * FROM AntiSpywareProduct", antiSpywareInfo));
    wmiLookups.push_back(WmiLookup(L"SELECT * FROM FirewallProduct", firewallInfo));

    for (vector<WmiLookup>::const_iterator it = wmiLookups.begin();
         it != wmiLookups.end();
         it++)
    {
        BSTR queryLanguage = SysAllocString(L"WQL");
        BSTR query = SysAllocString(it->query.c_str());

        IEnumWbemClassObject* pEnumerator = NULL;
        hr = pSvc->ExecQuery(
            queryLanguage, 
            query,
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
            NULL,
            &pEnumerator);

        SysFreeString(queryLanguage);
        SysFreeString(query);

        if (FAILED(hr))
        {
            pSvc->Release();
            pLoc->Release();
            CoUninitialize();
            return;
        }

        // Get the data from the query

        IWbemClassObject *pclsObj;
        ULONG uReturn = 0;

        while (pEnumerator)
        {
            hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

            if (0 == uReturn)
            {
                break;
            }

            // The available properties differ between the SecurityCenter and
            // SecurityCenter2 namespaces. For details see:
            // http://neophob.com/2010/03/wmi-query-windows-securitycenter2/

            SecurityInfo securityInfo;

            VARIANT vtProp;

            // The displayName property is common to the versions.
            hr = pclsObj->Get(L"displayName", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr))
            {
                securityInfo.displayName = vtProp.bstrVal;
                VariantClear(&vtProp);
            }

            if (securityCenter2)
            {
                securityInfo.version = _T("v2");

                VARIANT vtProp;
                hr = pclsObj->Get(L"productState", 0, &vtProp, 0, 0);

                if (SUCCEEDED(hr))
                {
                    securityInfo.v2.productState = vtProp.uintVal;
                    VariantClear(&vtProp);

                    // See the link above for details on what we're doing here.

                    // Product state

                    DWORD flags = 0;

                    // From Wscapi.h
                    enum
                    {
                        // Represents the aggregation of all firewalls for this computer.
                        WSC_SECURITY_PROVIDER_FIREWALL =                   0x1,
                        // Represents the Automatic updating settings for this computer.
                        WSC_SECURITY_PROVIDER_AUTOUPDATE_SETTINGS  =       0x2,
                        // Represents the aggregation of all antivirus products for this comptuer.
                        WSC_SECURITY_PROVIDER_ANTIVIRUS =                  0x4,
                        // Represents the aggregation of all antispyware products for this comptuer.
                        WSC_SECURITY_PROVIDER_ANTISPYWARE =                0x8,
                        // Represents the settings that restrict the access of web sites in each of the internet zones.
                        WSC_SECURITY_PROVIDER_INTERNET_SETTINGS =          0x10,
                        // Represents the User Account Control settings on this machine.
                        WSC_SECURITY_PROVIDER_USER_ACCOUNT_CONTROL =       0x20,
                        // Represents the running state of the Security Center service on this machine.
                        WSC_SECURITY_PROVIDER_SERVICE =                    0x40,

                        WSC_SECURITY_PROVIDER_NONE =                       0
                    };

                    flags = (securityInfo.v2.productState >> 16) & 0xFF;

                    if (flags == 0)
                    {
                        securityInfo.v2.securityProvider = _T("WSC_SECURITY_PROVIDER_NONE|");
                    }

                    if (flags  & WSC_SECURITY_PROVIDER_FIREWALL)
                    {
                        securityInfo.v2.securityProvider += _T("WSC_SECURITY_PROVIDER_FIREWALL|");
                    }

                    if (flags  & WSC_SECURITY_PROVIDER_AUTOUPDATE_SETTINGS)
                    {
                        securityInfo.v2.securityProvider += _T("WSC_SECURITY_PROVIDER_AUTOUPDATE_SETTINGS|");
                    }
            
                    if (flags  & WSC_SECURITY_PROVIDER_ANTIVIRUS)
                    {
                        securityInfo.v2.securityProvider += _T("WSC_SECURITY_PROVIDER_ANTIVIRUS|");
                    }
            
                    if (flags  & WSC_SECURITY_PROVIDER_ANTISPYWARE)
                    {
                        securityInfo.v2.securityProvider += _T("WSC_SECURITY_PROVIDER_ANTISPYWARE|");
                    }
            
                    if (flags  & WSC_SECURITY_PROVIDER_INTERNET_SETTINGS)
                    {
                        securityInfo.v2.securityProvider += _T("WSC_SECURITY_PROVIDER_INTERNET_SETTINGS|");
                    }
            
                    if (flags  & WSC_SECURITY_PROVIDER_USER_ACCOUNT_CONTROL)
                    {
                        securityInfo.v2.securityProvider += _T("WSC_SECURITY_PROVIDER_USER_ACCOUNT_CONTROL|");
                    }
            
                    if (flags  & WSC_SECURITY_PROVIDER_SERVICE)
                    {
                        securityInfo.v2.securityProvider += _T("WSC_SECURITY_PROVIDER_SERVICE|");
                    }

                    if (securityInfo.v2.securityProvider.length() > 0)
                    {
                        // Strip the trailing "|"
                        securityInfo.v2.securityProvider.resize(securityInfo.v2.securityProvider.size()-1);
                    }

                    // Scanner enabled

                    flags = (securityInfo.v2.productState >> 8) & 0xFF;

                    if (flags == 0x10 || flags == 0x11)
                    {
                        securityInfo.v2.enabled = true;
                    }
                    else // if (flags == 0x00 || flags == 0x01)
                    {
                        securityInfo.v2.enabled = false;
                    }

                    // Up-to-date

                    flags = securityInfo.v2.productState & 0xFF;

                    if (flags == 0x00)
                    {
                        securityInfo.v2.definitionsUpToDate = true;
                    }
                    else // if (flags == 0x10)
                    {
                        securityInfo.v2.definitionsUpToDate = false;
                    }
                }
            }
            else 
            {
                securityInfo.version = _T("v1");

                hr = pclsObj->Get(L"productUpToDate", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr))
                {
                    securityInfo.v1.productUpToDate = !!vtProp.boolVal;
                    VariantClear(&vtProp);
                }

                hr = pclsObj->Get(L"onAccessScanningEnabled", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr))
                {
                    securityInfo.v1.enabled = !!vtProp.boolVal;
                    VariantClear(&vtProp);
                }

                hr = pclsObj->Get(L"versionNumber", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr))
                {
                    securityInfo.v1.versionNumber = vtProp.bstrVal;
                    VariantClear(&vtProp);
                }
            }

            pclsObj->Release();

            it->results.push_back(securityInfo);
        }

        pEnumerator->Release();
    }

    // Cleanup

    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
}


struct StartupDiagnosticInfo {
    bool wininet_success;
    WininetNetworkInfo wininet_info;
} g_startupDiagnosticInfo;

// NOTE: Not threadsafe
void DoStartupDiagnosticCollection()
{
    // Reset
    g_startupDiagnosticInfo = StartupDiagnosticInfo();

    WininetNetworkInfo netInfo;
    g_startupDiagnosticInfo.wininet_success = false;
    if (WininetGetNetworkInfo(netInfo))
    {
        g_startupDiagnosticInfo.wininet_success = true;
        g_startupDiagnosticInfo.wininet_info = netInfo;
    }
}


/**
Adds diagnostic info to `o_json`.
*/
void GetDiagnosticInfo(Json::Value& o_json)
{
    o_json = Json::Value(Json::objectValue);

    /*
     * SystemInformation
     */

    o_json["SystemInformation"] = Json::Value(Json::objectValue);

    Json::Value psiphonInfo = Json::Value(Json::objectValue);
    psiphonInfo["PROPAGATION_CHANNEL_ID"] = PROPAGATION_CHANNEL_ID;
    psiphonInfo["SPONSOR_ID"] = SPONSOR_ID;
    psiphonInfo["CLIENT_VERSION"] = CLIENT_VERSION;
    psiphonInfo["splitTunnel"] = Settings::SplitTunnel();
    psiphonInfo["selectedTransport"] = WStringToUTF8(Settings::Transport());
    o_json["SystemInformation"]["PsiphonInfo"] = psiphonInfo;

    /*
    * SystemInformation::OSInfo
    */

    SystemInfo sysInfo;
    // We'll fill in the values even if this call fails.
    (void)GetSystemInfo(sysInfo);
    Json::Value osInfo = Json::Value(Json::objectValue);
    osInfo["name"] = WStringToUTF8(sysInfo.name);
    osInfo["version"] = WStringToUTF8(sysInfo.version);
    osInfo["codeSet"] = WStringToUTF8(sysInfo.codeSet);
    osInfo["countryCode"] = WStringToUTF8(sysInfo.countryCode);
    osInfo["freePhysicalMemoryKB"] = sysInfo.freePhysicalMemoryKB;
    osInfo["freeVirtualMemoryKB"] = sysInfo.freeVirtualMemoryKB;
    osInfo["locale"] = WStringToUTF8(sysInfo.locale);
    osInfo["architecture"] = WStringToUTF8(sysInfo.architecture);
    osInfo["language"] = sysInfo.language;
    osInfo["servicePackMajor"] = sysInfo.servicePackMajor;
    osInfo["servicePackMinor"] = sysInfo.servicePackMinor;
    osInfo["status"] = WStringToUTF8(sysInfo.status);
    osInfo["starter"] = sysInfo.starter;
    osInfo["mshtmlDLLVersion"] = WStringToUTF8(sysInfo.mshtmlDLLVersion);
    o_json["SystemInformation"]["OSInfo"] = osInfo;

    /*
    * SystemInformation::NetworkInfo
    */

    Json::Value networkInfo = Json::Value(Json::objectValue);

    networkInfo["Current"] = Json::Value(Json::objectValue);

    networkInfo["Current"]["Internet"] = Json::Value(Json::objectValue);
    if (sysInfo.wininet_success)
    {
        networkInfo["Current"]["Internet"]["internetConnected"] = true;
        networkInfo["Current"]["Internet"]["internetConnectionConfigured"] = sysInfo.wininet_info.internetConnectionConfigured;
        networkInfo["Current"]["Internet"]["internetConnectionLAN"] = sysInfo.wininet_info.internetConnectionLAN;
        networkInfo["Current"]["Internet"]["internetConnectionModem"] = sysInfo.wininet_info.internetConnectionModem;
        networkInfo["Current"]["Internet"]["internetConnectionOffline"] = sysInfo.wininet_info.internetConnectionOffline;
        networkInfo["Current"]["Internet"]["internetConnectionProxy"] = sysInfo.wininet_info.internetConnectionProxy;
        networkInfo["Current"]["Internet"]["internetRASInstalled"] = sysInfo.wininet_info.internetRASInstalled;
    }
    else 
    {
        networkInfo["Current"]["Internet"]["internetConnected"] = Json::nullValue;
        networkInfo["Current"]["Internet"]["internetConnectionConfigured"] = Json::nullValue;
        networkInfo["Current"]["Internet"]["internetConnectionLAN"] = Json::nullValue;
        networkInfo["Current"]["Internet"]["internetConnectionModem"] = Json::nullValue;
        networkInfo["Current"]["Internet"]["internetConnectionOffline"] = Json::nullValue;
        networkInfo["Current"]["Internet"]["internetConnectionProxy"] = Json::nullValue;
        networkInfo["Current"]["Internet"]["internetRASInstalled"] = Json::nullValue;
    }

    networkInfo["Original"] = Json::Value(Json::objectValue);

    networkInfo["Original"]["Proxy"] = Json::Value(Json::arrayValue);

    vector<ConnectionProxy> originalProxyInfo;
    GetSanitizedOriginalProxyInfo(originalProxyInfo);
    for (vector<ConnectionProxy>::const_iterator it = originalProxyInfo.begin();
         it != originalProxyInfo.end();
         it++)
    {
        Json::Value originalProxyInfoEntry(Json::objectValue);
        originalProxyInfoEntry["connectionName"] = WStringToUTF8(it->name);
        originalProxyInfoEntry["flags"] = WStringToUTF8(it->flagsString);
        originalProxyInfoEntry["proxy"] = WStringToUTF8(it->proxy);
        originalProxyInfoEntry["bypass"] = WStringToUTF8(it->bypass);

        networkInfo["Original"]["Proxy"].append(originalProxyInfoEntry);
    }

    networkInfo["Original"]["Internet"] = Json::Value(Json::objectValue);
    if (g_startupDiagnosticInfo.wininet_success)
    {
        networkInfo["Original"]["Internet"]["internetConnected"] = true;
        networkInfo["Original"]["Internet"]["internetConnectionConfigured"] = g_startupDiagnosticInfo.wininet_info.internetConnectionConfigured;
        networkInfo["Original"]["Internet"]["internetConnectionLAN"] = g_startupDiagnosticInfo.wininet_info.internetConnectionLAN;
        networkInfo["Original"]["Internet"]["internetConnectionModem"] = g_startupDiagnosticInfo.wininet_info.internetConnectionModem;
        networkInfo["Original"]["Internet"]["internetConnectionOffline"] = g_startupDiagnosticInfo.wininet_info.internetConnectionOffline;
        networkInfo["Original"]["Internet"]["internetConnectionProxy"] = g_startupDiagnosticInfo.wininet_info.internetConnectionProxy;
        networkInfo["Original"]["Internet"]["internetRASInstalled"] = g_startupDiagnosticInfo.wininet_info.internetRASInstalled;
    }
    else 
    {
        networkInfo["Original"]["Internet"]["internetConnected"] = Json::nullValue;
        networkInfo["Original"]["Internet"]["internetConnectionConfigured"] = Json::nullValue;
        networkInfo["Original"]["Internet"]["internetConnectionLAN"] = Json::nullValue;
        networkInfo["Original"]["Internet"]["internetConnectionModem"] = Json::nullValue;
        networkInfo["Original"]["Internet"]["internetConnectionOffline"] = Json::nullValue;
        networkInfo["Original"]["Internet"]["internetConnectionProxy"] = Json::nullValue;
        networkInfo["Original"]["Internet"]["internetRASInstalled"] = Json::nullValue;
    }

    o_json["SystemInformation"]["NetworkInfo"] = networkInfo;

    /*
    * SystemInformation::UserInfo
    */

    Json::Value userInfo = Json::Value(Json::objectValue);

    if (sysInfo.groupInfo_success)
    {
        userInfo["inAdminsGroup"] = sysInfo.groupInfo.inAdminsGroup;
        userInfo["inUsersGroup"] = sysInfo.groupInfo.inUsersGroup;
        userInfo["inGuestsGroup"] = sysInfo.groupInfo.inGuestsGroup;
        userInfo["inPowerUsersGroup"] = sysInfo.groupInfo.inPowerUsersGroup;
    }
    else 
    {
        userInfo["inAdminsGroup"] = Json::nullValue;
        userInfo["inUsersGroup"] = Json::nullValue;
        userInfo["inGuestsGroup"] = Json::nullValue;
        userInfo["inPowerUsersGroup"] = Json::nullValue;
    }

    o_json["SystemInformation"]["UserInfo"] = userInfo;

    /*
    * SystemInformation::SecurityInfo
    */

    Json::Value securityInfo = Json::Value(Json::objectValue);

    vector<SecurityInfo> antiVirusInfo, antiSpywareInfo, firewallInfo;
    GetOSSecurityInfo(antiVirusInfo, antiSpywareInfo, firewallInfo);

    struct SecurityInfoSet {
        string name;
        vector<SecurityInfo>& results;
        SecurityInfoSet(string name, vector<SecurityInfo>& results) : name(name), results(results) {}
    };
    vector<SecurityInfoSet> securityInfoSets;
    securityInfoSets.push_back(SecurityInfoSet("AntiVirusInfo", antiVirusInfo));
    securityInfoSets.push_back(SecurityInfoSet("AntiSpywareInfo", antiSpywareInfo));
    securityInfoSets.push_back(SecurityInfoSet("FirewallInfo", firewallInfo));

    for (vector<SecurityInfoSet>::const_iterator securityInfoSet = securityInfoSets.begin();
         securityInfoSet != securityInfoSets.end();
         securityInfoSet++)
    {
        Json::Value securityInfoSetJson(Json::arrayValue);

        for (vector<SecurityInfo>::const_iterator it = securityInfoSet->results.begin();
             it != securityInfoSet->results.end();
             it++)
        {
            Json::Value resultJson(Json::objectValue);

            resultJson["displayName"] = WStringToUTF8(it->displayName);
            resultJson["version"] = WStringToUTF8(it->version);
            
            resultJson["v1"] = Json::Value(Json::objectValue);
            resultJson["v1"]["productUpToDate"] = it->v1.productUpToDate;
            resultJson["v1"]["enabled"] = it->v1.enabled;
            resultJson["v1"]["versionNumber"] = WStringToUTF8(it->v1.versionNumber);

            resultJson["v2"] = Json::Value(Json::objectValue);
            resultJson["v2"]["productState"] = (Json::Int)it->v2.productState;
            resultJson["v2"]["securityProvider"] = WStringToUTF8(it->v2.securityProvider);
            resultJson["v2"]["enabled"] = it->v2.enabled;
            resultJson["v2"]["definitionsUpToDate"] = it->v2.definitionsUpToDate;

            securityInfoSetJson.append(resultJson);
        }

        securityInfo[securityInfoSet->name] = securityInfoSetJson;
    }

    o_json["SystemInformation"]["SecurityInfo"] = securityInfo;

    /*
    * SystemInformation::Misc
    */

    Json::Value miscInfo = Json::Value(Json::objectValue);

    miscInfo["mideastEnabled"] = sysInfo.mideastEnabled;
    miscInfo["slowMachine"] = sysInfo.slowMachine;

    o_json["SystemInformation"]["Misc"] = miscInfo;

    /*
     * Status History
     */

    Json::Value statusHistory = Json::Value(Json::arrayValue);

    vector<MessageHistoryEntry> messageHistory;
    GetMessageHistory(messageHistory);
    for (vector<MessageHistoryEntry>::const_iterator entry = messageHistory.begin();
         entry != messageHistory.end();
         entry++)
    {
        Json::Value messageEntry(Json::objectValue);
        messageEntry["message"] = WStringToUTF8(entry->message);
        messageEntry["debug"] = entry->debug;
        messageEntry["timestamp!!timestamp"] = WStringToUTF8(entry->timestamp);
        
        statusHistory.append(messageEntry);
    }

    o_json["StatusHistory"] = statusHistory;
}


bool SendFeedbackAndDiagnosticInfo(
        const string& feedback, 
        const string& emailAddress,
        const string& surveyJSON,
        bool sendDiagnosticInfo, 
        const StopInfo& stopInfo)
{
    if (feedback.empty() && !sendDiagnosticInfo)
    {
        // nothing to do
        return true;
    }

    CryptoPP::AutoSeededRandomPool rng;
    const size_t randBytesLen = 8;
    byte randBytes[randBytesLen];
    rng.GenerateBlock(randBytes, randBytesLen);
    string feedbackID = Hexlify(randBytes, randBytesLen);

    Json::Value outJson(Json::objectValue);

    // Metadata
    outJson["Metadata"] = Json::Value(Json::objectValue);
    outJson["Metadata"]["platform"] = "windows";
    outJson["Metadata"]["version"] = 2;
    outJson["Metadata"]["id"] = feedbackID;

    // Diagnostic info
    if (sendDiagnosticInfo)
    {
        outJson["DiagnosticInfo"] = Json::Value(Json::objectValue);
        GetDiagnosticInfo(outJson["DiagnosticInfo"]);
        
        outJson["DiagnosticInfo"]["DiagnosticHistory"] = Json::Value(Json::arrayValue);
        GetDiagnosticHistory(outJson["DiagnosticInfo"]["DiagnosticHistory"]);
    }

    // Feedback
    // NOTE: If the user supplied an email address but no feedback, then the
    // email address is discarded.
    if (!feedback.empty() || !surveyJSON.empty())
    {
        outJson["Feedback"] = Json::Value(Json::objectValue);

        outJson["Feedback"]["email"] = emailAddress;

        outJson["Feedback"]["Message"] = Json::Value(Json::objectValue);
        outJson["Feedback"]["Message"]["text"] = feedback;

        outJson["Feedback"]["Survey"] = Json::Value(Json::objectValue);
        outJson["Feedback"]["Survey"]["json"] = surveyJSON;
    }

    //
    // Upload the feedback/diagnostic info 
    //

    Json::FastWriter jsonWriter;
    string outJsonString = jsonWriter.write(outJson);

    string encryptedPayload;
    if (!PublicKeyEncryptData(
            FEEDBACK_ENCRYPTION_PUBLIC_KEY, 
            outJsonString.c_str(),
            encryptedPayload))
    {
        return false;
    }

    tstring uploadLocation = UTF8ToWString(FEEDBACK_DIAGNOSTIC_INFO_UPLOAD_PATH)
                                + UTF8ToWString(feedbackID);
        
    string response;
    HTTPSRequest httpsRequest;
    if (!httpsRequest.MakeRequest(
            UTF8ToWString(FEEDBACK_DIAGNOSTIC_INFO_UPLOAD_SERVER).c_str(),
            443,
            string(), // Do standard cert validation
            uploadLocation.c_str(),
            response,
            stopInfo,
            false, // don't use local proxy
            true,  // fail over to URL proxy
            UTF8ToWString(FEEDBACK_DIAGNOSTIC_INFO_UPLOAD_SERVER_HEADERS).c_str(),
            (LPVOID)encryptedPayload.c_str(),
            encryptedPayload.length(),
            _T("PUT")))
    {
        return false;
    }

    return true;
}
