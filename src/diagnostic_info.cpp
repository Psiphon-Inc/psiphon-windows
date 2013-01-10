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
#include "psiclient.h"
#include "embeddedvalues.h"
#include "httpsrequest.h"
#include "yaml-cpp/yaml.h"
#include "server_list_reordering.h"
#include "wininet_network_check.h"
#include "systemproxysettings.h"
#include "utilities.h"
#include "diagnostic_info.h"


static string GetOSVersionString()
{
    string output;

    OSVERSIONINFOEX osvi;
    BOOL bOsVersionInfoEx;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) )
    {
        osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
        if (!GetVersionEx ( (OSVERSIONINFO *) &osvi) ) 
        {
            return output;
        }
    }
    switch (osvi.dwPlatformId)
    {
    case VER_PLATFORM_WIN32_NT:
        if ( osvi.dwMajorVersion <= 4 ) output += ("Microsoft Windows NT ");
        if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 ) output += ("Microsoft Windows 2000 ");
        if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 ) output += ("Microsoft Windows XP ");
        if( bOsVersionInfoEx )
        {
            if ( osvi.wProductType == VER_NT_WORKSTATION )
            {
                if( osvi.wSuiteMask & VER_SUITE_PERSONAL ) output += ( "Personal " );
                else output += ( "Professional " );
            }
            else if ( osvi.wProductType == VER_NT_SERVER )
            {
                if( osvi.wSuiteMask & VER_SUITE_DATACENTER ) output += ( "DataCenter Server " );
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE ) output += ( "Advanced Server " );
                else output += ( "Server " );
            }
        }
        else
        {
            HKEY hKey;
            char szProductType[80];
            DWORD dwBufLen;
            RegOpenKeyExA( HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\ProductOptions", 0, KEY_QUERY_VALUE, &hKey );
            RegQueryValueExA( hKey, "ProductType", NULL, NULL, (LPBYTE) szProductType, &dwBufLen);
            RegCloseKey( hKey );
            if ( lstrcmpiA( "WINNT", szProductType) == 0 ) output += ( "Professional " );
            if ( lstrcmpiA( "LANMANNT", szProductType) == 0 ) output += ( "Server " );
            if ( lstrcmpiA( "SERVERNT", szProductType) == 0 ) output += ( "Advanced Server " );
        }

        if ( osvi.dwMajorVersion <= 4 )
        {
            std::ostringstream ss;
            ss << "version " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion << " " << osvi.szCSDVersion << "(Build " << (osvi.dwBuildNumber & 0xFFFF) << ")";
            output += ss.str();
        }
        else
        { 
            std::ostringstream ss;
            ss << osvi.szCSDVersion << " (Build " << (osvi.dwBuildNumber & 0xFFFF) << ")";
            output += ss.str();
        }
        break;

    case VER_PLATFORM_WIN32_WINDOWS:
        if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
        {
            output += ("Microsoft Windows 95 ");
            if ( osvi.szCSDVersion[1] == 'C' || osvi.szCSDVersion[1] == 'B' ) output += ("OSR2 " );
        } 

        if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
        {
            output += ("Microsoft Windows 98 ");
            if ( osvi.szCSDVersion[1] == 'A' ) output += ("SE " );
        }
        if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
        {
            output += ("Microsoft Windows Me ");
        } 
        break;

    case VER_PLATFORM_WIN32s:
        output += ("Microsoft Win32s ");
        break;
    }

    return output; 
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
    OSVERSIONINFO osver = { sizeof(osver) };
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
    string name;
    DWORD platformId;
    DWORD majorVersion;
    DWORD minorVersion;
    DWORD productType;
    WORD servicePackMajor;
    WORD servicePackMinor;
    DWORD edition;
    DWORD suiteMask;
    string csdVersion;
    DWORD buildNumber;
    bool starter;
    bool mideastEnabled;
    bool slowMachine;
    bool wininet_success;
    WininetNetworkInfo wininet_info;
    bool userIsAdmin;
    bool groupInfo_success;
    UserGroupInfo groupInfo;
    vector<ConnectionProxyInfo> originalProxyInfo;
};

// Adapted from http://msdn.microsoft.com/en-us/library/windows/desktop/ms724429%28v=vs.85%29.aspx
bool GetSystemInfo(SystemInfo& sysInfo)
{
    typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
    typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

    OSVERSIONINFOEX osvi;
    SYSTEM_INFO si;
    PGNSI pGNSI;
    PGPI pGPI;
    BOOL bOsVersionInfoEx;
    DWORD dwType;

    ostringstream ss;

    ZeroMemory(&si, sizeof(SYSTEM_INFO));
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO*) &osvi);

    if (bOsVersionInfoEx == 0) 
    {
        return false;
    }

    // Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.

    pGNSI = (PGNSI)GetProcAddress(
                        GetModuleHandle(TEXT("kernel32.dll")), 
                        "GetNativeSystemInfo");
    if(NULL != pGNSI)
    {
        pGNSI(&si);
    }
    else 
    {
        GetSystemInfo(&si);
    }

    if (VER_PLATFORM_WIN32_NT == osvi.dwPlatformId && osvi.dwMajorVersion > 4)
    {
        ss << "Microsoft ";

        // Test for the specific product.

        if (osvi.dwMajorVersion >= 6)
        {
            if (osvi.dwMinorVersion == 0)
            {
                if (osvi.wProductType == VER_NT_WORKSTATION)
                {
                    ss << "Windows Vista ";
                }
                else 
                {
                    ss << "Windows Server 2008 ";
                }
            }
            else if (osvi.dwMinorVersion == 1)
            {
                if (osvi.wProductType == VER_NT_WORKSTATION)
                {
                    ss << "Windows 7 ";
                }
                else 
                {
                    ss << "Windows Server 2008 R2 ";
                }
            }
            else if (osvi.dwMinorVersion == 2)
            {
                if (osvi.wProductType == VER_NT_WORKSTATION)
                {
                    ss << "Windows 8 ";
                }
                else 
                {
                    ss << "Windows Server 2012 ";
                }
            }

            pGPI = (PGPI)GetProcAddress(
                            GetModuleHandle(TEXT("kernel32.dll")), 
                            "GetProductInfo");

            pGPI(osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.wServicePackMajor, osvi.wServicePackMinor, &dwType);

            switch(dwType)
            {
            case PRODUCT_ULTIMATE:
                ss << "Ultimate Edition";
                break;
            case PRODUCT_PROFESSIONAL:
                ss << "Professional";
                break;
            case PRODUCT_HOME_PREMIUM:
                ss << "Home Premium Edition";
                break;
            case PRODUCT_HOME_BASIC:
                ss << "Home Basic Edition";
                break;
            case PRODUCT_ENTERPRISE:
                ss << "Enterprise Edition";
                break;
            case PRODUCT_BUSINESS:
                ss << "Business Edition";
                break;
            case PRODUCT_STARTER:
                ss << "Starter Edition";
                break;
            case PRODUCT_CLUSTER_SERVER:
                ss << "Cluster Server Edition";
                break;
            case PRODUCT_DATACENTER_SERVER:
                ss << "Datacenter Edition";
                break;
            case PRODUCT_DATACENTER_SERVER_CORE:
                ss << "Datacenter Edition (core installation)";
                break;
            case PRODUCT_ENTERPRISE_SERVER:
                ss << "Enterprise Edition";
                break;
            case PRODUCT_ENTERPRISE_SERVER_CORE:
                ss << "Enterprise Edition (core installation)";
                break;
            case PRODUCT_ENTERPRISE_SERVER_IA64:
                ss << "Enterprise Edition for Itanium-based Systems";
                break;
            case PRODUCT_SMALLBUSINESS_SERVER:
                ss << "Small Business Server";
                break;
            case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
                ss << "Small Business Server Premium Edition";
                break;
            case PRODUCT_STANDARD_SERVER:
                ss << "Standard Edition";
                break;
            case PRODUCT_STANDARD_SERVER_CORE:
                ss << "Standard Edition (core installation)";
                break;
            case PRODUCT_WEB_SERVER:
                ss << "Web Server Edition";
                break;
            }
        }

        if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2)
        {
            if (GetSystemMetrics(SM_SERVERR2))
            {
                ss << "Windows Server 2003 R2, ";
            }
            else if (osvi.wSuiteMask & VER_SUITE_STORAGE_SERVER)
            {
                ss << "Windows Storage Server 2003";
            }
            else if (osvi.wSuiteMask & VER_SUITE_WH_SERVER)
            {
                ss << "Windows Home Server";
            }
            else if (osvi.wProductType == VER_NT_WORKSTATION &&
                     si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
            {
                ss << "Windows XP Professional x64 Edition";
            }
            else 
            {
                ss << "Windows Server 2003, ";
            }

            // Test for the server type.
            if (osvi.wProductType != VER_NT_WORKSTATION)
            {
                if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64)
                {
                    if (osvi.wSuiteMask & VER_SUITE_DATACENTER)
                    {
                        ss << "Datacenter Edition for Itanium-based Systems";
                    }
                    else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
                    {
                        ss << "Enterprise Edition for Itanium-based Systems";
                    }
                }

                else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
                {
                    if (osvi.wSuiteMask & VER_SUITE_DATACENTER)
                    {
                        ss << "Datacenter x64 Edition";
                    }
                    else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
                    {
                        ss << "Enterprise x64 Edition";
                    }
                    else
                    {
                        ss << "Standard x64 Edition";
                    }
                }

                else
                {
                    if (osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER)
                    {
                        ss << "Compute Cluster Edition";
                    }
                    else if (osvi.wSuiteMask & VER_SUITE_DATACENTER)
                    {
                        ss << "Datacenter Edition";
                    }
                    else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
                    {
                        ss << "Enterprise Edition";
                    }
                    else if (osvi.wSuiteMask & VER_SUITE_BLADE)
                    {
                        ss << "Web Edition";
                    }
                    else 
                    {
                        ss << "Standard Edition";
                    }
                }
            }
        }

        if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1)
        {
            ss << "Windows XP ";
            if (osvi.wSuiteMask & VER_SUITE_PERSONAL)
            {
                ss << "Home Edition";
            }
            else
            {
                ss << "Professional";
            }
        }

        if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0)
        {
            ss << "Windows 2000 ";

            if (osvi.wProductType == VER_NT_WORKSTATION)
            {
                ss << "Professional" ;
            }
            else 
            {
                if (osvi.wSuiteMask & VER_SUITE_DATACENTER)
                {
                    ss << "Datacenter Server";
                }
                else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
                {
                    ss << "Advanced Server";
                }
                else 
                {
                    ss << "Server";
                }
            }
        }

        // Include service pack (if any) and build number.

        if (_tcslen(osvi.szCSDVersion) > 0)
        {
            ss << " ";
            ss << osvi.szCSDVersion;
        }

        ss << " (build " << osvi.dwBuildNumber << ")";

        if (osvi.dwMajorVersion >= 6)
        {
            if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
            {
                ss << " 64-bit";
            }
            else if (si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL)
            {
                ss << " 32-bit";
            }
        }

        sysInfo.name = ss.str(); 
        sysInfo.platformId = osvi.dwPlatformId;
        sysInfo.majorVersion = osvi.dwMajorVersion;
        sysInfo.minorVersion = osvi.dwMinorVersion;
        sysInfo.productType = osvi.wProductType;
        sysInfo.servicePackMajor = osvi.wServicePackMajor;
        sysInfo.servicePackMinor = osvi.wServicePackMinor;
        sysInfo.edition = dwType;
        sysInfo.suiteMask = osvi.wSuiteMask;
        sysInfo.csdVersion = TStringToNarrow(osvi.szCSDVersion);
        sysInfo.buildNumber = osvi.dwBuildNumber;
        sysInfo.starter = (GetSystemMetrics(SM_STARTER) != 0);

        sysInfo.mideastEnabled = (GetSystemMetrics(SM_MIDEASTENABLED) != 0);
        sysInfo.slowMachine = (GetSystemMetrics(SM_SLOWMACHINE) != 0);

        WininetNetworkInfo netInfo;
        sysInfo.wininet_success = false;
        if (WininetGetNetworkInfo(netInfo))
        {
            sysInfo.wininet_success = true;
            sysInfo.wininet_info = netInfo;
        }

        sysInfo.groupInfo_success = false;
        if (GetUserGroupInfo(sysInfo.groupInfo))
        {
            sysInfo.groupInfo_success = true;
        }

        GetOriginalProxyInfo(sysInfo.originalProxyInfo);

        return true;
    }

    return false;
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
}

string GetDiagnosticInfo(const string& diagnosticInfoID)
{
    YAML::Emitter out;
        
    out << YAML::BeginMap; // overall

    /*
     * Metadata
     */

    out << YAML::Key << "Metadata";
    out << YAML::Value;
    out << YAML::BeginMap; // metadata
    out << YAML::Key << "platform" << YAML::Value << "windows";
    out << YAML::Key << "version" << YAML::Value << 1;
    out << YAML::Key << "id" << YAML::Value << diagnosticInfoID;
    out << YAML::EndMap; // metadata

    /*
     * System Information
     */

    out << YAML::Key << "SystemInformation";
    out << YAML::Value;
    out << YAML::BeginMap; // sysinfo

    out << YAML::Key << "psiphonEmbeddedValues";
    out << YAML::Value;
    out << YAML::BeginMap; // embedded
    out << YAML::Key << "PROPAGATION_CHANNEL_ID";
    out << YAML::Value << PROPAGATION_CHANNEL_ID;
    out << YAML::Key << "SPONSOR_ID";
    out << YAML::Value << SPONSOR_ID;
    out << YAML::Key << "CLIENT_VERSION";
    out << YAML::Value << CLIENT_VERSION;
    out << YAML::EndMap; // embedded

    SystemInfo sysInfo;
    // We'll fill in the values even if this call fails.
    (void)GetSystemInfo(sysInfo);
    out << YAML::Key << "OSInfo";
    out << YAML::Value;
    out << YAML::BeginMap; // osinfo
    out << YAML::Key << "name" << YAML::Value << sysInfo.name.c_str();
    out << YAML::Key << "platformId" << YAML::Value << sysInfo.platformId;
    out << YAML::Key << "majorVersion" << YAML::Value << sysInfo.majorVersion;
    out << YAML::Key << "minorVersion" << YAML::Value << sysInfo.minorVersion;
    out << YAML::Key << "productType" << YAML::Value << sysInfo.productType;
    out << YAML::Key << "servicePackMajor" << YAML::Value << sysInfo.servicePackMajor;
    out << YAML::Key << "servicePackMinor" << YAML::Value << sysInfo.servicePackMinor;
    out << YAML::Key << "ediition" << YAML::Value << sysInfo.edition;
    out << YAML::Key << "suiteMask" << YAML::Value << sysInfo.suiteMask;
    out << YAML::Key << "csdVersion" << YAML::Value << sysInfo.csdVersion.c_str();
    out << YAML::Key << "buildNumber" << YAML::Value << sysInfo.buildNumber;
    out << YAML::Key << "starter" << YAML::Value << sysInfo.starter;
    out << YAML::EndMap; // osinfo

    out << YAML::Key << "NetworkInfo";
    out << YAML::Value;
    out << YAML::BeginMap; // netinfo
    if (sysInfo.wininet_success)
    {
        out << YAML::Key << "internetConnected" << YAML::Value << true;
        out << YAML::Key << "internetConnectionConfigured" << YAML::Value << sysInfo.wininet_info.internetConnectionConfigured;
        out << YAML::Key << "internetConnectionLAN" << YAML::Value << sysInfo.wininet_info.internetConnectionLAN;
        out << YAML::Key << "internetConnectionModem" << YAML::Value << sysInfo.wininet_info.internetConnectionModem;
        out << YAML::Key << "internetConnectionOffline" << YAML::Value << sysInfo.wininet_info.internetConnectionOffline;
        out << YAML::Key << "internetConnectionProxy" << YAML::Value << sysInfo.wininet_info.internetConnectionProxy;
        out << YAML::Key << "internetRASInstalled" << YAML::Value << sysInfo.wininet_info.internetRASInstalled;
    }
    else 
    {
        out << YAML::Key << "internetConnected" << YAML::Value << false;
        out << YAML::Key << "internetConnectionConfigured" << YAML::Value << YAML::Null;
        out << YAML::Key << "internetConnectionLAN" << YAML::Value << YAML::Null;
        out << YAML::Key << "internetConnectionModem" << YAML::Value << YAML::Null;
        out << YAML::Key << "internetConnectionOffline" << YAML::Value << YAML::Null;
        out << YAML::Key << "internetConnectionProxy" << YAML::Value << YAML::Null;
        out << YAML::Key << "internetRASInstalled" << YAML::Value << YAML::Null;
    }

    out << YAML::Key << "OriginalProxyInfo";
    out << YAML::Value;
    out << YAML::BeginSeq;
    for (vector<ConnectionProxyInfo>::const_iterator it = sysInfo.originalProxyInfo.begin();
         it != sysInfo.originalProxyInfo.end();
         it++)
    {
        out << YAML::BeginMap; // OriginalProxyInfo
        out << YAML::Key << "connectionName" << YAML::Value << TStringToNarrow(it->connectionName).c_str();
        out << YAML::Key << "flags" << YAML::Value << TStringToNarrow(it->flags).c_str();
        out << YAML::Key << "proxy" << YAML::Value << TStringToNarrow(it->proxy).c_str();
        out << YAML::Key << "bypass" << YAML::Value << TStringToNarrow(it->bypass).c_str();
        out << YAML::EndMap; // OriginalProxyInfo
    }
    out << YAML::EndSeq;

    out << YAML::EndMap; // netinfo

    out << YAML::Key << "UserInfo";
    out << YAML::Value;
    out << YAML::BeginMap; //userinfo
    if (sysInfo.groupInfo_success)
    {
        out << YAML::Key << "inAdminsGroup" << YAML::Value << sysInfo.groupInfo.inAdminsGroup;
        out << YAML::Key << "inUsersGroup" << YAML::Value << sysInfo.groupInfo.inUsersGroup;
        out << YAML::Key << "inGuestsGroup" << YAML::Value << sysInfo.groupInfo.inGuestsGroup;
        out << YAML::Key << "inPowerUsersGroup" << YAML::Value << sysInfo.groupInfo.inPowerUsersGroup;
    }
    else 
    {
        out << YAML::Key << "internetConnected" << YAML::Value << false;
        out << YAML::Key << "internetConnectionConfigured" << YAML::Value << YAML::Null;
        out << YAML::Key << "internetConnectionLAN" << YAML::Value << YAML::Null;
        out << YAML::Key << "internetConnectionModem" << YAML::Value << YAML::Null;
        out << YAML::Key << "internetConnectionOffline" << YAML::Value << YAML::Null;
        out << YAML::Key << "internetConnectionProxy" << YAML::Value << YAML::Null;
        out << YAML::Key << "internetRASInstalled" << YAML::Value << YAML::Null;
    }
    out << YAML::EndMap; // userinfo

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

    out << YAML::Key << "SecurityInfo";
    out << YAML::Value;
    out << YAML::BeginMap; // SecurityInfo

    for (vector<SecurityInfoSet>::const_iterator securityInfoSet = securityInfoSets.begin();
         securityInfoSet != securityInfoSets.end();
         securityInfoSet++)
    {
        out << YAML::Key << securityInfoSet->name.c_str();
        out << YAML::Value;
        out << YAML::BeginSeq;
        for (vector<SecurityInfo>::const_iterator it = securityInfoSet->results.begin();
             it != securityInfoSet->results.end();
             it++)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "displayName" << YAML::Value << TStringToNarrow(it->displayName).c_str();
            out << YAML::Key << "version" << YAML::Value << TStringToNarrow(it->version).c_str();
            out << YAML::Key << "v1";
            out << YAML::Value;
            out << YAML::BeginMap;
            out << YAML::Key << "productUpToDate" << YAML::Value << it->v1.productUpToDate;
            out << YAML::Key << "enabled" << YAML::Value << it->v1.enabled;
            out << YAML::Key << "versionNumber" << YAML::Value << TStringToNarrow(it->v1.versionNumber).c_str();
            out << YAML::EndMap;
            out << YAML::Key << "v2";
            out << YAML::Value;
            out << YAML::BeginMap;
            out << YAML::Key << "productState" << YAML::Value << it->v2.productState;
            out << YAML::Key << "securityProvider" << YAML::Value << TStringToNarrow(it->v2.securityProvider).c_str();
            out << YAML::Key << "enabled" << YAML::Value << it->v2.enabled;
            out << YAML::Key << "definitionsUpToDate" << YAML::Value << it->v2.definitionsUpToDate;
            out << YAML::EndMap;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
    }

    out << YAML::EndMap; // SecurityInfo

    out << YAML::Key << "Misc";
    out << YAML::Value;
    out << YAML::BeginMap; // misc
    out << YAML::Key << "mideastEnabled" << YAML::Value << sysInfo.mideastEnabled;
    out << YAML::Key << "slowMachine" << YAML::Value << sysInfo.slowMachine;
    out << YAML::EndMap; // misc


    out << YAML::EndMap; // sysinfo

    /*
     * Server Response Check
     */

    out << YAML::Key << "ServerResponseCheck";
    out << YAML::Value;
    out << YAML::BeginSeq;

    vector<ServerReponseCheck> serverReponseCheckHistory;
    GetServerResponseCheckHistory(serverReponseCheckHistory);
    for (vector<ServerReponseCheck>::const_iterator entry = serverReponseCheckHistory.begin();
         entry != serverReponseCheckHistory.end();
         entry++)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "ipAddress" << YAML::Value << entry->serverAddress.c_str();
        out << YAML::Key << "responded" << YAML::Value << entry->responded;
        out << YAML::Key << "responseTime" << YAML::Value << entry->responseTime;
        out << YAML::Key << "timestamp" << YAML::Value << entry->timestamp.c_str();
        out << YAML::EndMap;
    }

    out << YAML::EndSeq;

    /*
     * Server Response Check
     */

    out << YAML::Key << "StatusHistory";
    out << YAML::Value;
    out << YAML::BeginSeq;

    vector<MessageHistoryEntry> messageHistory;
    GetMessageHistory(messageHistory);
    for (vector<MessageHistoryEntry>::const_iterator entry = messageHistory.begin();
         entry != messageHistory.end();
         entry++)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "message" << YAML::Value << TStringToNarrow(entry->message).c_str();
        out << YAML::Key << "debug" << YAML::Value << entry->debug;
        out << YAML::Key << "timestamp" << YAML::Value << TStringToNarrow(entry->timestamp).c_str();
        out << YAML::EndMap;
    }

    out << YAML::EndSeq;


    out << YAML::EndMap; // overall

    return out.c_str();
}

bool OpenEmailAndSendDiagnosticInfo(
        const string& emailAddress, 
        const string& emailAddressEncoded, 
        const string& diagnosticInfoID, 
        const StopInfo& stopInfo)
{
    if (emailAddress.length() > 0)
    {
        assert(emailAddressEncoded.length() > 0);
        //
        // First put the address into the clipboard
        //

        if (!OpenClipboard(NULL))
        {
            return false;
        }

        // Remove the current Clipboard contents 
        if( !EmptyClipboard() )
        {
            return false;
        }
   
        // Get the currently selected data
        HGLOBAL hGlob = GlobalAlloc(GMEM_FIXED, emailAddress.length()+1);
        strcpy_s((char*)hGlob, emailAddress.length()+1, emailAddress.c_str());
    
        // Note that the system takes ownership of hGlob
        if (::SetClipboardData( CF_TEXT, hGlob ) == NULL)
        {
            CloseClipboard();
            GlobalFree(hGlob);
            return false;
        }

        CloseClipboard();

        //
        // Launch the email handler
        //

        string command = "mailto:" + emailAddress;

        (void)::ShellExecuteA( 
                    NULL, 
                    "open", 
                    command.c_str(), 
                    NULL, 
                    NULL, 
                    SW_SHOWNORMAL); 

        // TODO: What does ShellExecute return if there's no registered mailto handler?
        // For now: Don't bother checking the return value at all. We've copied the
        // address to the clipboard and that will have to be good enough.
    }

    //
    // Upload the diagnostic info
    //

    if (diagnosticInfoID.length() > 0)
    {
        string diagnosticInfo = GetDiagnosticInfo(diagnosticInfoID);

        string encryptedPayload;
        if (!PublicKeyEncryptData(
                FEEDBACK_ENCRYPTION_PUBLIC_KEY, 
                diagnosticInfo.c_str(), 
                encryptedPayload))
        {
            return false;
        }

        tstring uploadLocation = NarrowToTString(FEEDBACK_DIAGNOSTIC_INFO_UPLOAD_PATH)
                                    + NarrowToTString(diagnosticInfoID);
        
        string response;
        HTTPSRequest httpsRequest;
        if (!httpsRequest.MakeRequest(
                NarrowToTString(FEEDBACK_DIAGNOSTIC_INFO_UPLOAD_SERVER).c_str(),
                443,
                string(), // Do standard cert validation
                uploadLocation.c_str(),
                response,
                stopInfo,
                false, // don't use local proxy
                NarrowToTString(FEEDBACK_DIAGNOSTIC_INFO_UPLOAD_SERVER_HEADERS).c_str(),
                (LPVOID)encryptedPayload.c_str(),
                encryptedPayload.length(),
                _T("PUT")))
        {
            return false;
        }
    }

    return true;
}
