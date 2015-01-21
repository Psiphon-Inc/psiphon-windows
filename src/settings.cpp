/*
* Copyright (c) 2015, Psiphon Inc.
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
#include "resource.h"
#include "settings.h"
#include "utilities.h"

#define SPLIT_TUNNEL_REGISTRY_NAME      "SplitTunnel"
#define SPLIT_TUNNEL_DEFAULT            FALSE

static HANDLE g_registryMutex = CreateMutex(NULL, FALSE, 0);

void Load(HWND hDlg)
{
    AutoMUTEX lock(g_registryMutex);

    DWORD splitTunnel;
    if (!ReadRegistryDwordValue(SPLIT_TUNNEL_REGISTRY_NAME, splitTunnel))
    {
        splitTunnel = SPLIT_TUNNEL_DEFAULT;
    }
    SendMessage(GetDlgItem(hDlg, IDC_SPLIT_TUNNEL_CHECKBOX), BM_SETCHECK, splitTunnel ? BST_CHECKED : BST_UNCHECKED, 0);
}

void Save(HWND hDlg)
{
    AutoMUTEX lock(g_registryMutex);

    bool splitTunnel = BST_CHECKED == SendMessage(GetDlgItem(hDlg, IDC_SPLIT_TUNNEL_CHECKBOX), BM_GETCHECK, 0, 0);
    WriteRegistryDwordValue(SPLIT_TUNNEL_REGISTRY_NAME, splitTunnel ? TRUE : FALSE);
}

INT_PTR CALLBACK SettingsDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        Load(hDlg);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            Save(hDlg);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void Settings::Show(HINSTANCE hInst, HWND hParentWnd)
{
    DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS_DLG), hParentWnd, SettingsDlg);
}

bool Settings::SplitTunnel()
{
    AutoMUTEX lock(g_registryMutex);

    DWORD splitTunnel;
    if (!ReadRegistryDwordValue(SPLIT_TUNNEL_REGISTRY_NAME, splitTunnel))
    {
        splitTunnel = SPLIT_TUNNEL_DEFAULT;
    }
    return !!splitTunnel;
}
