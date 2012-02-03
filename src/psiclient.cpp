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

//==== Includes ===============================================================

#include "stdafx.h"

// This is for Windows XP/Vista+ style controls
#include <Commctrl.h>
#pragma comment (lib, "Comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "psiclient.h"
#include "connectionmanager.h"
#include "embeddedvalues.h"
#include "transport.h"


//==== Globals ================================================================

#define MAX_LOADSTRING 100

HINSTANCE g_hInst;
TCHAR g_szTitle[MAX_LOADSTRING];
TCHAR g_szWindowClass[MAX_LOADSTRING];

HWND g_hWnd;
ConnectionManager g_connectionManager;
tstring g_lastTransportSelection;

// (...more globals in Controls section)


//==== UI layout ===============================================================

//
//                 + - - - - - - - - - - - +
//                 ' ^                     '
// [toggle button] ' | transport selection ' [banner]
//                 ' v                     '
//                 + - - - - - - - - - - - +
// +------------------------------------------------+
// | ^                                              |
// | | log list box                                 |
// | v                                              |
// +------------------------------------------------+
//                   [info link]
//

const int SPACER = 5;

const int TOGGLE_BUTTON_X = 0 + SPACER;
const int TOGGLE_BUTTON_IMAGE_WIDTH = 48;
const int TOGGLE_BUTTON_WIDTH = 56;
const int TOGGLE_BUTTON_HEIGHT = 56;

const int TRANSPORT_FIRST_ITEM_X = TOGGLE_BUTTON_X + TOGGLE_BUTTON_WIDTH + SPACER;
const int TRANSPORT_FIRST_ITEM_Y = 0 + SPACER;
const int TRANSPORT_ITEM_WIDTH = 48;
const int TRANSPORT_ITEM_HEIGHT = 16;

// First transport in this list is the default

const TCHAR* transportOptions[] = {_T("SSH+"), _T("VPN"), _T("SSH")};
const int transportOptionCount = sizeof(transportOptions)/sizeof(const TCHAR*);

const int TRANSPORT_TOTAL_HEIGHT = TRANSPORT_ITEM_HEIGHT + (transportOptionCount-1)*(SPACER + TRANSPORT_ITEM_HEIGHT);

// Toggle button and banner are vertically centered relative to transport section

const int TOGGLE_BUTTON_Y = 0 + SPACER + (TRANSPORT_TOTAL_HEIGHT > TOGGLE_BUTTON_HEIGHT ?
                                          (TRANSPORT_TOTAL_HEIGHT - TOGGLE_BUTTON_HEIGHT)/2 : 0);

const int BANNER_X = TRANSPORT_FIRST_ITEM_X + TRANSPORT_ITEM_WIDTH + SPACER;
const int BANNER_WIDTH = 200;
const int BANNER_HEIGHT = 48;

const int BANNER_Y = 0 + SPACER + (TRANSPORT_TOTAL_HEIGHT > BANNER_HEIGHT ?
                                          (TRANSPORT_TOTAL_HEIGHT - BANNER_HEIGHT)/2 : 0);

const int WINDOW_WIDTH = BANNER_X + BANNER_WIDTH + SPACER + 20; // non-client-area hack adjustment
const int WINDOW_HEIGHT = 200;

const int INFO_LINK_WIDTH = 200;
const int INFO_LINK_HEIGHT = 16;
const int INFO_LINK_X = 0 + (WINDOW_WIDTH - INFO_LINK_WIDTH)/2;
const int INFO_LINK_Y = WINDOW_HEIGHT - INFO_LINK_HEIGHT;

const int LOG_LIST_BOX_X = 0;
const int LOG_LIST_BOX_Y = max(TOGGLE_BUTTON_HEIGHT,
                               max(BANNER_HEIGHT,
                                   transportOptionCount*(TRANSPORT_ITEM_HEIGHT+SPACER))) + SPACER;
const int LOG_LIST_BOX_WIDTH = WINDOW_WIDTH;
const int LOG_LIST_BOX_HEIGHT = WINDOW_HEIGHT - (LOG_LIST_BOX_Y + SPACER + INFO_LINK_HEIGHT);


//==== Controls ================================================================

HWND g_hToggleButton = NULL;
HIMAGELIST g_hToggleButtonImageList = NULL;
const int TOGGLE_BUTTON_ICON_COUNT = 6;
HICON g_hToggleButtonIcons[TOGGLE_BUTTON_ICON_COUNT];
HWND g_hBannerStatic = NULL;
HBITMAP g_hBannerBitmap = NULL;
HBITMAP g_hEmailBitmap = NULL;
HWND g_hTransportRadioButtons[transportOptionCount];
HWND g_hLogListBox = NULL;
HWND g_hInfoLinkStatic = NULL;
bool g_bShowEmail = false;


void ResizeControls(HWND hWndParent)
{
    RECT rect;
    GetClientRect(hWndParent, &rect);

    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    if (g_hLogListBox != NULL)
    {
        MoveWindow(
            g_hLogListBox,
            LOG_LIST_BOX_X,
            LOG_LIST_BOX_Y,
            windowWidth,
            windowHeight - (LOG_LIST_BOX_Y + SPACER + INFO_LINK_HEIGHT),
            TRUE);
    }

    if (g_hInfoLinkStatic != NULL)
    {
        MoveWindow(
            g_hInfoLinkStatic,
            (windowWidth - INFO_LINK_WIDTH)/2,
            windowHeight - INFO_LINK_HEIGHT,
            INFO_LINK_WIDTH,
            INFO_LINK_HEIGHT,
            TRUE);
    }
}


void CreateControls(HWND hWndParent)
{
    HGDIOBJ font = GetStockObject(DEFAULT_GUI_FONT);

    // Toggle Button

    g_hToggleButton = CreateWindow(
        L"Button",
        L"",
        WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_ICON,
        TOGGLE_BUTTON_X,
        TOGGLE_BUTTON_Y,
        TOGGLE_BUTTON_WIDTH,
        TOGGLE_BUTTON_HEIGHT,
        hWndParent,
        (HMENU)IDC_TOGGLE_BUTTON,
        g_hInst,
        NULL);

    g_hToggleButtonImageList = ImageList_LoadImage(
        g_hInst,
        MAKEINTRESOURCE(IDB_TOGGLE_BUTTON_IMAGES),
        TOGGLE_BUTTON_IMAGE_WIDTH,
        0,
        CLR_DEFAULT,
        IMAGE_BITMAP,
        LR_CREATEDIBSECTION);

    assert(TOGGLE_BUTTON_ICON_COUNT == ImageList_GetImageCount(g_hToggleButtonImageList));

    for (int i = 0; i < TOGGLE_BUTTON_ICON_COUNT; i++)
    {
        g_hToggleButtonIcons[i] = ImageList_GetIcon(
            g_hToggleButtonImageList,
            i,
            ILD_NORMAL);
    }

    // Banner

    g_hBannerStatic = CreateWindow(
        L"Static",
        0,
        WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE|SS_BITMAP|SS_NOTIFY,
        BANNER_X,
        BANNER_Y,
        BANNER_WIDTH,
        BANNER_HEIGHT,
        hWndParent,
        (HMENU)IDC_BANNER_STATIC,
        g_hInst,
        NULL);
    g_hBannerBitmap = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BANNER));
    g_hEmailBitmap = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_EMAIL));
    SendMessage(g_hBannerStatic, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)g_hBannerBitmap);
    EnableWindow(g_hBannerStatic, TRUE);
    ShowWindow(g_hBannerStatic, TRUE);

    // Transport Radio Buttons

    for (int i = 0; i < transportOptionCount; i++)
    {
        g_hTransportRadioButtons[i] = CreateWindow(
            L"Button",
            transportOptions[i],
            WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON|(i == 0 ? WS_GROUP : 0),
            TRANSPORT_FIRST_ITEM_X + SPACER,
            TRANSPORT_FIRST_ITEM_Y + i*(TRANSPORT_ITEM_HEIGHT + SPACER),
            TRANSPORT_ITEM_WIDTH,
            TRANSPORT_ITEM_HEIGHT,
            hWndParent,
            (HMENU)(IDC_TRANSPORT_OPTION_RADIO_FIRST + i),
            g_hInst,
            NULL);

        SendMessage(g_hTransportRadioButtons[i], WM_SETFONT, (WPARAM)font, NULL);

        if (i == 0)
        {
            SendMessage(
                g_hTransportRadioButtons[i],
                BM_SETCHECK,
                BST_CHECKED,
                0);
        }
    }

    // Log List

    g_hLogListBox = CreateWindow(
        L"Listbox",
        L"",
        WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOINTEGRALHEIGHT|LBS_DISABLENOSCROLL|LBS_NOTIFY,
        LOG_LIST_BOX_X,
        LOG_LIST_BOX_Y,
        LOG_LIST_BOX_WIDTH,
        LOG_LIST_BOX_HEIGHT,
        hWndParent,
        (HMENU)IDC_LOG_LISTBOX,
        g_hInst,
        NULL);
    SendMessage(g_hLogListBox, WM_SETFONT, (WPARAM)font, NULL);

    // Info Link

    g_hInfoLinkStatic = CreateWindow(
        L"Static",
        L"https://psiphon3.com", // TODO: ...
        WS_CHILD|WS_VISIBLE|SS_NOTIFY,
        INFO_LINK_X,
        INFO_LINK_Y,
        INFO_LINK_WIDTH,
        INFO_LINK_HEIGHT,
        hWndParent,
        (HMENU)IDC_INFO_LINK_STATIC,
        g_hInst,
        NULL);
    SendMessage(g_hInfoLinkStatic, WM_SETFONT, (WPARAM)font, NULL);
}


void UpdateButton(HWND hWndParent)
{
    static ConnectionManagerState g_lastState = g_connectionManager.GetState();

    static int g_nextAnimationIndex = 0;
    int iconIndex = 0;

    ConnectionManagerState state = g_connectionManager.GetState();

    // Flash the taskbar after disconnected

    if (state == CONNECTION_MANAGER_STATE_STOPPED && state != g_lastState)
    {
        FLASHWINFO info;
        info.cbSize = sizeof(FLASHWINFO);
        info.hwnd = hWndParent;
        info.dwFlags = FLASHW_ALL|FLASHW_TIMERNOFG;
        info.uCount = 1;
        info.dwTimeout = 0;
        FlashWindowEx(&info);
    }

    g_lastState = state;

    // Update the button

    if (state == CONNECTION_MANAGER_STATE_STOPPED)
    {
        iconIndex = 0;
    }
    else if (state == CONNECTION_MANAGER_STATE_CONNECTED)
    {
        iconIndex = 1;
    }
    else /* if CONNECTION_MANAGER_STATE_STARTING */
    {
        iconIndex = 2 + (g_nextAnimationIndex++)%4;
    }

    HANDLE currentIcon = (HANDLE)SendMessage(
        g_hToggleButton,
        BM_GETIMAGE,
        IMAGE_ICON,
        0);

    if (currentIcon != g_hToggleButtonIcons[iconIndex])
    {
        SendMessage(
            g_hToggleButton,
            BM_SETIMAGE,
            IMAGE_ICON,
            (LPARAM)g_hToggleButtonIcons[iconIndex]);
    }
}


void UpdateBanner(HWND hWndParent)
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
             state == CONNECTION_MANAGER_STATE_CONNECTED)
    {
        g_bShowEmail = false;
    }

    HBITMAP hBitmap = g_hBannerBitmap;

    if (g_bShowEmail)
    {
        hBitmap = g_hEmailBitmap;
    }

    if (hBitmap != (HBITMAP)SendMessage(g_hBannerStatic, STM_GETIMAGE, (WPARAM)IMAGE_BITMAP, 0))
    {
        SendMessage(g_hBannerStatic, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)hBitmap);
    }
}


tstring GetSelectedTransport(void)
{
    for (int i = 0; i < transportOptionCount; i++)
    {
        if (BST_CHECKED == SendMessage(g_hTransportRadioButtons[i], BM_GETCHECK, 0, 0))
        {
            return transportOptions[i];
        }
    }
    assert(0);
    return _T("");
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


//==== Win32 boilerplate ======================================================

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPTSTR lpCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadString(hInstance, IDS_APP_TITLE, g_szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_PSICLIENT, g_szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization

    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable;
    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PSICLIENT));

    // Main message loop

    MSG msg;
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
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    //wcex.lpszMenuName = MAKEINTRESOURCE(IDC_PSICLIENT);
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = g_szWindowClass;
    wcex.hIconSm = NULL;

    return RegisterClassEx(&wcex);
}


//==== Main window function ===================================================

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   g_hInst = hInstance;

   RECT rect;
   SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);

   g_hWnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        g_szWindowClass,
        g_szTitle,
        WS_OVERLAPPEDWINDOW,
        rect.right - WINDOW_WIDTH,
        rect.bottom - WINDOW_HEIGHT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL);

   ShowWindow(g_hWnd, nCmdShow);
   UpdateWindow(g_hWnd);

   return TRUE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;
    TCHAR* myPrintMessage;

    switch (message)
    {
    case WM_CREATE:
        CreateControls(hWnd);

        // Display client version number 

        SendMessage(
            g_hLogListBox,
            LB_ADDSTRING,
            NULL,
            (LPARAM)(tstring(_T("Client Version: ")) + NarrowToTString(CLIENT_VERSION)).c_str());

        // NOTE: we leave the connection animation timer running even after fully connected
        // when the icon no longer animates -- since the timer handler also updates the UI
        // when unexpectedly disconnected.
        SetTimer(hWnd, IDT_BUTTON_ANIMATION, 250, NULL);

        // Start a connection on the default transport

        g_lastTransportSelection = GetSelectedTransport();
        g_connectionManager.Toggle(g_lastTransportSelection);

        break;

    case WM_SIZE:
        ResizeControls(hWnd);
        break;

    case WM_TIMER:
        if (IDT_BUTTON_ANIMATION == wParam)
        {
            UpdateButton(hWnd);
            UpdateBanner(hWnd);
        }
        break;

    case WM_COMMAND:

        wmId = LOWORD(wParam);
        wmEvent = HIWORD(wParam);

        // lParam == 0: menu or accelerator event

        if (lParam == 0)
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
                DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        // lParam != 0: control notifications

        // Toggle button clicked

        else if (lParam == (LPARAM)g_hToggleButton && wmEvent == BN_CLICKED)
        {
            my_print(true, _T("%s: Button pressed, Toggle called"), __TFUNCTION__);

            // See comment below about Stop() blocking the UI
            SetCursor(LoadCursor(0, IDC_WAIT));

            g_connectionManager.Toggle(GetSelectedTransport());
        }

        // Transport radio button clicked

        else if (lParam != 0
                 && wmId >= IDC_TRANSPORT_OPTION_RADIO_FIRST && wmId <= IDC_TRANSPORT_OPTION_RADIO_LAST
                 && wmEvent == BN_CLICKED)
        {
            tstring newTransportSelection = GetSelectedTransport();

            if (newTransportSelection != g_lastTransportSelection)
            {
                // Restart with the new transport immediately

                // Show a Wait cursor since ConnectionManager::Stop() (called by Start) can
                // take a few seconds to complete, which blocks the radio button redrawing
                // animation. WM_SETCURSOR will reset the cursor automatically.
                SetCursor(LoadCursor(0, IDC_WAIT));

                g_connectionManager.Start(newTransportSelection);

                g_lastTransportSelection = newTransportSelection;
            }
        }
        
        // Banner clicked
        
        else if (lParam == (LPARAM)g_hBannerStatic && wmEvent == STN_CLICKED)
        {
            int state = g_connectionManager.GetState();
            if (CONNECTION_MANAGER_STATE_CONNECTED == state)
            {
                g_connectionManager.OpenHomePages();
            }
        }

        // Info link clicked
        
        else if (lParam == (LPARAM)g_hInfoLinkStatic && wmEvent == STN_CLICKED)
        {
            // Info link static control was clicked, so open Psiphon 3 page
            // TODO: ...
        }
        break;

    case WM_PSIPHON_MY_PRINT:
        // Display message in listbox and scroll to bottom of listbox.
        myPrintMessage = (TCHAR*)lParam;
        SendMessage(g_hLogListBox, LB_ADDSTRING, NULL, (LPARAM)myPrintMessage);
        OutputDebugString(myPrintMessage);
        OutputDebugString(L"\n");
        free(myPrintMessage);
        SendMessage(g_hLogListBox, LB_SETCURSEL,
        SendMessage(g_hLogListBox, LB_GETCOUNT, NULL, NULL)-1, NULL);
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


//==== About box ==============================================================

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
