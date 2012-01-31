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

//==== includes ===============================================================

#include "stdafx.h"
#include "psiclient.h"
#include "connectionmanager.h"
#include "embeddedvalues.h"


//==== layout =================================================================

// TODO: Calculate instead of using magic constants

const int BUTTON_SIZE = 48;
const int BANNER_X = (BUTTON_SIZE + 10);
const int BANNER_Y = 3;
const int BANNER_WIDTH = 200;
const int BANNER_HEIGHT = BUTTON_SIZE;
const int VPN_DISABLED_X = (BUTTON_SIZE + 10) + BANNER_WIDTH;
const int VPN_DISABLED_Y = 3;
const int VPN_DISABLED_WIDTH = BUTTON_SIZE;
const int VPN_DISABLED_HEIGHT = BUTTON_SIZE;
const int TOOLBAR_HEIGHT = BUTTON_SIZE + 16;
const int WINDOW_WIDTH = (BUTTON_SIZE + 10)*2 + BANNER_WIDTH + 10;
const int WINDOW_HEIGHT = 130;


//==== Win32 boilerplate ======================================================

#define MAX_LOADSTRING 100

// Global Variables
HINSTANCE hInst;
TCHAR szTitle[MAX_LOADSTRING];
TCHAR szWindowClass[MAX_LOADSTRING];

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MSG msg;
    HACCEL hAccelTable;

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_PSICLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PSICLIENT));

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PSICLIENT));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    //wcex.lpszMenuName = MAKEINTRESOURCE(IDC_PSICLIENT);
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = NULL;

    return RegisterClassEx(&wcex);
}

HWND g_hWnd;

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;
   RECT rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};

   hInst = hInstance; // Store instance handle in our global variable

   SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);

   hWnd = CreateWindowEx(
            WS_EX_APPWINDOW,
            szWindowClass,
            szTitle,
            WS_OVERLAPPEDWINDOW,
            // CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
            rect.right - WINDOW_WIDTH, rect.bottom - WINDOW_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT,
            NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   g_hWnd = hWnd;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}


//==== The Connection Manager =================================================

ConnectionManager g_connectionManager;


//==== toolbar ================================================================

// http://msdn.microsoft.com/en-us/library/bb760446%28v=VS.85%29.aspx


HWND g_hToolBar = NULL;
HWND g_hBanner = NULL;
HBITMAP g_hBannerBitmap = NULL;
HBITMAP g_hEmailBitmap = NULL;
HWND g_hVPNSkipped = NULL;
HIMAGELIST g_hToolbarImageList = NULL;
bool g_bShowEmail = false;

void CreateToolbar(HWND hWndParent)
{
    // Define some constants.
    const int ImageListID = 0;
    const int numButtons = 1;
    const int bitmapSize = BUTTON_SIZE;

    // Create the toolbar.
    g_hToolBar = CreateWindowEx(
                            0, TOOLBARCLASSNAME, NULL, 
                            WS_CHILD | TBSTYLE_WRAPABLE,
                            0, 0, 0, 0,
                            hWndParent, NULL, hInst, NULL);
    if (g_hToolBar == NULL)
    {
        return;
    }

    // Create image list from bitmap

    g_hToolbarImageList = ImageList_LoadImage(
        hInst, MAKEINTRESOURCE(IDB_TOOLBAR_ICONS),
        bitmapSize, numButtons, CLR_DEFAULT, // GetSysColor(COLOR_BTNFACE),
        IMAGE_BITMAP, LR_CREATEDIBSECTION);

    // Set the image list.
    SendMessage(
        g_hToolBar, TB_SETIMAGELIST, (WPARAM)ImageListID, 
        (LPARAM)g_hToolbarImageList);

    // Initialize button info.
    TBBUTTON tbButtons[numButtons] = 
    {
        { MAKELONG(0, ImageListID), IDM_TOGGLE, TBSTATE_ENABLED, 
          BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"" }
    };

    // Add buttons.
    SendMessage(
        g_hToolBar, TB_BUTTONSTRUCTSIZE, 
        (WPARAM)sizeof(TBBUTTON), 0);
    SendMessage(
        g_hToolBar, TB_ADDBUTTONS, (WPARAM)numButtons, 
        (LPARAM)&tbButtons);

    // Add banner child control.
    g_hBanner = CreateWindow(
                        L"Static", 0,
                        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_BITMAP | SS_NOTIFY,
                        BANNER_X, BANNER_Y, BANNER_WIDTH, BANNER_HEIGHT,
                        g_hToolBar, NULL, hInst, NULL);
    g_hBannerBitmap = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BANNER));
    g_hEmailBitmap = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_EMAIL));
    SendMessage(g_hBanner, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)g_hBannerBitmap);
    EnableWindow(g_hBanner, TRUE);
    ShowWindow(g_hBanner, TRUE);

    // Add VPN disabled child control.
    g_hVPNSkipped = CreateWindow(
                        L"Static", 0,
                        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_ICON | SS_NOTIFY,
                        VPN_DISABLED_X, VPN_DISABLED_Y, VPN_DISABLED_WIDTH, VPN_DISABLED_HEIGHT,
                        g_hToolBar, NULL, hInst, NULL);
     HICON hVPNSkippedIcon = ImageList_GetIcon(g_hToolbarImageList, 7, 0);
     SendMessage(g_hVPNSkipped, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hVPNSkippedIcon);
     EnableWindow(g_hVPNSkipped, FALSE);
     ShowWindow(g_hVPNSkipped, FALSE);

    // Tell the toolbar to resize itself, and show it.
    SendMessage(g_hToolBar, TB_AUTOSIZE, 0, 0); 
    ShowWindow(g_hToolBar, TRUE);
}

void UpdateButton(HWND hWnd)
{
    static ConnectionManagerState g_lastState = g_connectionManager.GetState();

    TBBUTTONINFO info;
    info.cbSize = sizeof(info);
    info.dwMask = TBIF_IMAGE;
    static int g_nextAnimationIndex = 0;
    int image = 0;

    ConnectionManagerState state = g_connectionManager.GetState();

    // Flash the taskbar after disconnected

    if (state == CONNECTION_MANAGER_STATE_STOPPED && state != g_lastState)
    {
        FLASHWINFO info;
        info.cbSize = sizeof(FLASHWINFO);
        info.hwnd = hWnd;
        info.dwFlags = FLASHW_ALL|FLASHW_TIMERNOFG;
        info.uCount = 1;
        info.dwTimeout = 0;
        FlashWindowEx(&info);
    }

    g_lastState = state;

    // Update the button

    if (state == CONNECTION_MANAGER_STATE_STOPPED)
    {
        image = 0;
    }
    else if (state == CONNECTION_MANAGER_STATE_CONNECTED_VPN)
    {
        image = 1;
    }
    else if (state == CONNECTION_MANAGER_STATE_CONNECTED_SSH)
    {
        image = 2;
    }
    else /* if CONNECTION_MANAGER_STATE_STARTING */
    {
        image = 3 + (g_nextAnimationIndex++)%4;
    }

    SendMessage(g_hToolBar, TB_GETBUTTONINFO, IDM_TOGGLE, (LPARAM)&info);

    if (info.iImage != image)
    {
        info.iImage = image;
        SendMessage(g_hToolBar, TB_SETBUTTONINFO, IDM_TOGGLE, (LPARAM)&info);
    }

    // Update the VPN skipped icon

    if (state == CONNECTION_MANAGER_STATE_CONNECTED_SSH &&
        g_connectionManager.CurrentSessionSkippedVPN())
    {
        EnableWindow(g_hVPNSkipped, TRUE);
        ShowWindow(g_hVPNSkipped, TRUE);
    }
    else
    {
        EnableWindow(g_hVPNSkipped, FALSE);
        ShowWindow(g_hVPNSkipped, FALSE);
    }
}

void UpdateAlpha(HWND hWnd)
{
    // Make window transparent when connected

    ConnectionManagerState state = g_connectionManager.GetState();
    if (state == CONNECTION_MANAGER_STATE_CONNECTED_VPN
        || state == CONNECTION_MANAGER_STATE_CONNECTED_SSH)
    {
        // Can also animate a fade out: http://msdn.microsoft.com/en-us/library/ms997507.aspx
        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hWnd, 0, (255 * 75) / 100, LWA_ALPHA);
    }
    else
    {
        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hWnd, 0, (255 * 100) / 100, LWA_ALPHA);
    }
}

void UpdateBanner(HWND hWnd)
{
    // Replace the sponsor banner with an image promoting email propagation:
    // - When starting takes more than N seconds
    // - After cancelling a start that took more than N seconds
    // The sponsor banner is restored on a sucessful connection and when the
    // start button is toggled again.

    ConnectionManagerState state = g_connectionManager.GetState();
    time_t startingTime = g_connectionManager.GetStartingTime();
    time_t timeUntilEmail = 35;

    if (state == CONNECTION_MANAGER_STATE_STARTING && startingTime > timeUntilEmail)
    {
        g_bShowEmail = true;
    }
    else if ((state == CONNECTION_MANAGER_STATE_STARTING && startingTime <= timeUntilEmail) ||
             state == CONNECTION_MANAGER_STATE_CONNECTED_VPN ||
             state == CONNECTION_MANAGER_STATE_CONNECTED_SSH)
    {
        g_bShowEmail = false;
    }

    HBITMAP hBitmap = g_hBannerBitmap;

    if (g_bShowEmail)
    {
        hBitmap = g_hEmailBitmap;
    }

    if (hBitmap != (HBITMAP)SendMessage(g_hBanner, STM_GETIMAGE, (WPARAM)IMAGE_BITMAP, 0))
    {
        SendMessage(g_hBanner, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)hBitmap);
    }
}

//==== my_print (logging) =====================================================

#ifdef _DEBUG
bool g_bShowDebugMessages = true;
#else
bool g_bShowDebugMessages = false;
#endif

void my_print(bool bDebugMessage, const TCHAR* format, ...)
{
    if (!bDebugMessage || g_bShowDebugMessages)
    {
        TCHAR* debugPrefix = _T("DEBUG: ");
        size_t debugPrefixLength = _tcsclen(debugPrefix);
        TCHAR* buffer = NULL;
        va_list args;
        va_start(args, format);
        int length = _vsctprintf(format, args) + 1;
        if (bDebugMessage)
        {
            length += debugPrefixLength;
        }
        buffer = (TCHAR*)malloc(length * sizeof(TCHAR));
        if (!buffer) return;
        if (bDebugMessage)
        {
            _tcscpy_s(buffer, length, debugPrefix);
            _vstprintf_s(buffer + debugPrefixLength, length - debugPrefixLength, format, args);
        }
        else
        {
            _vstprintf_s(buffer, length, format, args);
        }
        va_end(args);

        // NOTE:
        // Main window handles displaying the message. This avoids
        // deadlocks with SendMessage. Main window will deallocate
        // buffer.

        PostMessage(g_hWnd, WM_PSIPHON_MY_PRINT, NULL, (LPARAM)buffer);
    }
}

void my_print(bool bDebugMessage, const string& message)
{
    my_print(bDebugMessage, NarrowToTString(message).c_str());
}


//==== Main window function ===================================================

static UINT_PTR g_hTimer;
static HWND g_hListBox = NULL;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rect;
    HGDIOBJ font;
    TCHAR* myPrintMessage;

    switch (message)
    {
    case WM_CREATE:

        CreateToolbar(hWnd);

        g_hListBox = CreateWindow(_T("listbox"),
                                _T(""),
                                WS_CHILD|WS_VISIBLE|WS_VSCROLL|
                                LBS_NOINTEGRALHEIGHT|LBS_DISABLENOSCROLL|LBS_NOTIFY,
                                0, 0, 1, 1,
                                hWnd, NULL, NULL, NULL);
        font = GetStockObject(DEFAULT_GUI_FONT);
        SendMessage(g_hListBox, WM_SETFONT, (WPARAM)font, NULL);

        // Display client version number 

        SendMessage(g_hListBox, LB_ADDSTRING, NULL,
            (LPARAM)(tstring(_T("Client Version: ")) + NarrowToTString(CLIENT_VERSION)).c_str());

        // NOTE: we leave the connection animation timer running even after fully connected
        // when the icon no longer animates -- since the timer handler also updates the UI
        // when unexpectedly disconnected.
        g_hTimer = SetTimer(hWnd, IDT_BUTTON_ROTATION, 250, NULL);

        g_connectionManager.Toggle();

        break;

    case WM_TIMER:
        UpdateButton(hWnd);
        UpdateBanner(hWnd);
        // DISABLED: UpdateAlpha(hWnd);
        break;

    case WM_SIZE:
        // make list box fill window client area
        GetClientRect(hWnd, &rect);
        if (g_hToolBar != NULL)
        {
            MoveWindow(
                g_hToolBar,
                0, 0,
                rect.right-rect.left, TOOLBAR_HEIGHT,
                TRUE);
        }
        if (g_hListBox != NULL)
        {
            MoveWindow(
                g_hListBox,
                0, TOOLBAR_HEIGHT,
                rect.right-rect.left, rect.bottom-rect.top - TOOLBAR_HEIGHT,
                TRUE);
        }
        break;

    case WM_COMMAND:

        wmId = LOWORD(wParam);
        wmEvent = HIWORD(wParam);

        if (lParam == 0) // menu or accelerator event
        {
            switch (wmId)
            {
            case IDM_SHOW_DEBUG_MESSAGES:
                g_bShowDebugMessages = !g_bShowDebugMessages;
                my_print(false, _T("Show debug messages: %s"), g_bShowDebugMessages ? _T("Yes") : _T("No"));
                break;
            // TODO: remove help, about, and exit?  The menu is currently hidden
            case IDM_HELP:
                break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        // lParam != 0: control notifications
        else if (lParam == (LPARAM)g_hToolBar && wmId == IDM_TOGGLE)
        {
            my_print(true, _T("%s: Button pressed, Toggle called"), __TFUNCTION__);
            g_connectionManager.Toggle();
        }
        else if (lParam == (LPARAM)g_hBanner && wmEvent == STN_CLICKED)
        {
            // Banner static control was clicked, so open home pages
            int state = g_connectionManager.GetState();
            if (CONNECTION_MANAGER_STATE_CONNECTED_VPN == state
                || CONNECTION_MANAGER_STATE_CONNECTED_SSH == state)
            {
                g_connectionManager.OpenHomePages();
            }
        }
        else if (lParam == (LPARAM)g_hVPNSkipped && wmEvent == STN_CLICKED)
        {
            // "VPN Skipped" icon is displayed when connected to SSH
            // and VPN connection attempt was skipped due to previous
            // failure. In this case, when the user clicks the icon,
            // we disconnect, clear the skipped state, and
            // reconnect -- which by default will try
            // the same server but with VPN not skipped this time.
            int state = g_connectionManager.GetState();
            if (CONNECTION_MANAGER_STATE_CONNECTED_SSH == state)
            {
                g_connectionManager.Stop();
                g_connectionManager.ResetSkipVPN();
                g_connectionManager.Start();
            }
        }
        break;

    case WM_PSIPHON_MY_PRINT:
        // Display message in listbox and scroll to bottom of listbox.
        myPrintMessage = (TCHAR*)lParam;
        SendMessage(g_hListBox, LB_ADDSTRING, NULL, (LPARAM)myPrintMessage);
        OutputDebugString(myPrintMessage);
        OutputDebugString(L"\n");
        free(myPrintMessage);
        SendMessage(g_hListBox, LB_SETCURSEL,
        SendMessage(g_hListBox, LB_GETCOUNT, NULL, NULL)-1, NULL);
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;

    case WM_DESTROY:
        // Stop VPN if running
        g_connectionManager.Stop();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
