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


/**************************************************************************
   ShowHTMLDlg()
**************************************************************************/
int ShowHTMLDlg(
        HWND hParentWnd, 
        LPCTSTR resourceName, 
        LPCTSTR urlFragment,
        LPCWSTR args,
        wstring& o_result)
{
    o_result.clear();

    // Contrary to the documentation, passing NULL for pvarArgIn 
    // seems to always result in ShowHTMLDialog returning a "bad
    // variable type" error. So we're going to set args to empty 
    // string if not provided.
    if (!args) args = L"";

    bool error = false;

    HINSTANCE hinstMSHTML = LoadLibrary(TEXT("MSHTML.DLL"));

    if (hinstMSHTML)
    {
        SHOWHTMLDIALOGEXFN *pfnShowHTMLDialog;

        pfnShowHTMLDialog = (SHOWHTMLDIALOGEXFN*)GetProcAddress(hinstMSHTML, "ShowHTMLDialogEx");

        if (pfnShowHTMLDialog)
        {
            tstring url = ResourceToUrl(resourceName, NULL, urlFragment);

            IMoniker* pmk = NULL;
            CreateURLMonikerEx(NULL, url.c_str(), &pmk, URL_MK_UNIFORM);

            if (pmk)
            {
                HRESULT  hr;
                VARIANT  varArgs, varReturn;

                VariantInit(&varArgs);
                varArgs.vt = VT_BSTR;
                varArgs.bstrVal = SysAllocString(args);

                VariantInit(&varReturn);

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
                            o_result = wstring(varReturn.bstrVal, SysStringLen(varReturn.bstrVal));

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
ShowHTMLDlg()
**************************************************************************/
tstring ResourceToUrl(LPCTSTR resourceName, LPCTSTR urlQuery, LPCTSTR urlFragment)
{
    tstring url;

    url += _T("res://");

    TCHAR   szTemp[MAX_PATH * 2];
    GetModuleFileName(NULL, szTemp, ARRAYSIZE(szTemp));
    url += szTemp;

    url += _T("/");
    url += resourceName;

    // URI encoding seems to be taken care of automatically (fortuitous, but unsettling)

    if (urlQuery)
    {
        url += _T("?");
        url += urlQuery;
    }

    if (urlFragment)
    {
        url += _T("#");
        url += urlFragment;
    }

    return url;
}
