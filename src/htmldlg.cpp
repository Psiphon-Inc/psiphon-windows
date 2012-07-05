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

// Adapted from the MSDN htmldlg demo. Available here: http://www.microsoft.com/en-us/download/details.aspx?id=944

#include "stdafx.h"
#include "htmldlg.h"


int BSTRToLocal(LPTSTR pLocal, BSTR pWide, DWORD dwChars);
int LocalToBSTR(BSTR pWide, LPTSTR pLocal, DWORD dwChars);


/**************************************************************************

   ShowHTMLDlg()

**************************************************************************/

int ShowHTMLDlg(
        HWND hParentWnd, 
        LPCTSTR resourceName, 
        LPCTSTR args,
        tstring& o_result)
{
    o_result.clear();

    // Contrary to the documentation, passing NULL for pvarArgIn 
    // seems to always result in ShowHTMLDialog returning a "bad
    // variable type" error. So we're going to set args to empty 
    // string if not provided.
    if (!args) args = _T("");

    bool error = false;

    HINSTANCE hinstMSHTML = LoadLibrary(TEXT("MSHTML.DLL"));

    if (hinstMSHTML)
    {
        SHOWHTMLDIALOGEXFN *pfnShowHTMLDialog;

        pfnShowHTMLDialog = (SHOWHTMLDIALOGEXFN*)GetProcAddress(hinstMSHTML, "ShowHTMLDialogEx");

        if (pfnShowHTMLDialog)
        {
            IMoniker *pmk;
            TCHAR    szTemp[MAX_PATH*2];
            OLECHAR  bstr[MAX_PATH*2];

            lstrcpy(szTemp, TEXT("res://"));
            GetModuleFileName(NULL, szTemp + lstrlen(szTemp), ARRAYSIZE(szTemp) - lstrlen(szTemp));
            lstrcat(szTemp, _T("/"));
            lstrcat(szTemp, resourceName);

            LocalToBSTR(bstr, szTemp, ARRAYSIZE(bstr));

            CreateURLMonikerEx(NULL, bstr, &pmk, URL_MK_UNIFORM);

            if (pmk)
            {
                HRESULT  hr;
                VARIANT  varArgs, varReturn;

                VariantInit(&varReturn);
                varArgs.vt = VT_BSTR;
                varArgs.bstrVal = SysAllocString(args);

                hr = (*pfnShowHTMLDialog)(
                        hParentWnd, 
                        pmk, 
                        HTMLDLG_MODAL | HTMLDLG_VERIFY, 
                        &varArgs, 
                        L"resizable:yes;", 
                        &varReturn);

                VariantClear(&varArgs);

                pmk->Release();

                if (SUCCEEDED(hr))
                {
                    switch (varReturn.vt)
                    {
                    case VT_BSTR:
                        {
                            TCHAR szData[MAX_PATH];

                            BSTRToLocal(szData, varReturn.bstrVal, ARRAYSIZE(szData));

                            o_result = szData;

                            VariantClear(&varReturn);
                        }
                        break;

                    default:
                        // Dialog was cancelled.
                        break;
                    }
                }
                else
                {
                    error = true;
                }

            }
            else
            {
                error = true;
            }
        }
        else
        {
            error = true;
        }

        FreeLibrary(hinstMSHTML);
    }
    else
    {
        error = true;
    }

    if (error) return -1;
    return o_result.length() > 0 ? 1 : 0;
}


/**************************************************************************

   BSTRToLocal()
   
**************************************************************************/

int BSTRToLocal(LPTSTR pLocal, BSTR pWide, DWORD dwChars)
{
*pLocal = 0;

#ifdef UNICODE
lstrcpyn(pLocal, pWide, dwChars);
#else
WideCharToMultiByte( CP_ACP, 
                     0, 
                     pWide, 
                     -1, 
                     pLocal, 
                     dwChars, 
                     NULL, 
                     NULL);
#endif

return lstrlen(pLocal);
}


/**************************************************************************

   LocalToBSTR()
   
**************************************************************************/

int LocalToBSTR(BSTR pWide, LPTSTR pLocal, DWORD dwChars)
{
*pWide = 0;

#ifdef UNICODE
lstrcpyn(pWide, pLocal, dwChars);
#else
MultiByteToWideChar( CP_ACP, 
                     0, 
                     pLocal, 
                     -1, 
                     pWide, 
                     dwChars); 
#endif

return lstrlenW(pWide);
}
