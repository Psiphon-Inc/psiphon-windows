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

// This is for COM functions
# pragma comment(lib, "wbemuuid.lib")

#include "psiclient.h"
#include "connectionmanager.h"
#include "embeddedvalues.h"
#include "transport.h"
#include "config.h"
#include "utilities.h"
#include "webbrowser.h"
#include "limitsingleinstance.h"
#include "htmldlg.h"
#include "server_list_reordering.h"
#include "stopsignal.h"
#include "osrng.h"
#include "diagnostic_info.h"


//==== Globals ================================================================

#define MAX_LOADSTRING 100

HINSTANCE g_hInst;
TCHAR g_szTitle[MAX_LOADSTRING];
TCHAR g_szWindowClass[MAX_LOADSTRING];

HWND g_hWnd;
ConnectionManager g_connectionManager;
tstring g_lastTransportSelection;
ServerListReorder g_serverListReorder;

LimitSingleInstance g_singleInstanceObject(TEXT("Global\\{B88F6262-9CC8-44EF-887D-FB77DC89BB8C}"));

// (...more globals in Controls section)


//==== UI layout ===============================================================

//
//                 + - - - - - - - - - - - +           +----------+
//                 ' ^                     '           |          |
// [toggle button] ' | transport selection ' [banner]  | feedback |
//                 ' v                     '           | button   |  
//                 + - - - - - - - - - - - +           |          |
//                 [split tunnel check box.......]     +----------+ 
// +--------------------------------------------------------------+   
// | ^                                                            |   
// | | log list box                                               |   
// | v                                                            |   
// +--------------------------------------------------------------+   
//                   [info link]
//


const int SPACER = 5;

const int TOGGLE_BUTTON_X = 0 + SPACER;
const int TOGGLE_BUTTON_IMAGE_WIDTH = 48;
const int TOGGLE_BUTTON_WIDTH = 56;
const int TOGGLE_BUTTON_HEIGHT = 56;

// First transport in this list is the default

const TCHAR* transportOptions[] = {_T("SSH+"), _T("VPN"), _T("SSH")};
const int transportOptionCount = sizeof(transportOptions)/sizeof(const TCHAR*);

const int TRANSPORT_FIRST_ITEM_X = TOGGLE_BUTTON_X + TOGGLE_BUTTON_WIDTH + SPACER;
const int TRANSPORT_FIRST_ITEM_Y = 0 + SPACER;
const int TRANSPORT_ITEM_WIDTH = 16 + LongestTextWidth(transportOptions, transportOptionCount);
const int TRANSPORT_ITEM_HEIGHT = TextHeight();
const int TRANSPORT_TOTAL_HEIGHT = TRANSPORT_ITEM_HEIGHT + (transportOptionCount-1)*(SPACER + TRANSPORT_ITEM_HEIGHT);

// Toggle button and banner are vertically centered relative to transport section

const int TOGGLE_BUTTON_Y = 0 + SPACER + (TRANSPORT_TOTAL_HEIGHT > TOGGLE_BUTTON_HEIGHT ?
                                          (TRANSPORT_TOTAL_HEIGHT - TOGGLE_BUTTON_HEIGHT)/2 : 0);

const int BANNER_X = TRANSPORT_FIRST_ITEM_X + TRANSPORT_ITEM_WIDTH + SPACER;
const int BANNER_WIDTH = 200;
const int BANNER_HEIGHT = 48;

const int BANNER_Y = 0 + SPACER + (TRANSPORT_TOTAL_HEIGHT > BANNER_HEIGHT ?
                                          (TRANSPORT_TOTAL_HEIGHT - BANNER_HEIGHT)/2 : 0);

const TCHAR* splitTunnelPrompt = _T("Don't proxy domestic web sites");

const int SPLIT_TUNNEL_X = TRANSPORT_FIRST_ITEM_X + SPACER;
const int SPLIT_TUNNEL_Y = max(TOGGLE_BUTTON_HEIGHT,
                               max(BANNER_HEIGHT,
                                   transportOptionCount*(TRANSPORT_ITEM_HEIGHT+SPACER))) + SPACER;
const int SPLIT_TUNNEL_WIDTH = TextWidth(splitTunnelPrompt);
const int SPLIT_TUNNEL_HEIGHT = TextHeight() + SPACER;

const int FEEDBACK_BUTTON_IMAGE_WIDTH = 48;
const int FEEDBACK_BUTTON_WIDTH = 56;
const int FEEDBACK_BUTTON_HEIGHT = 56;
const int FEEDBACK_BUTTON_X = BANNER_X + BANNER_WIDTH + SPACER;
const int FEEDBACK_BUTTON_Y = TOGGLE_BUTTON_Y;

const int WINDOW_WIDTH = FEEDBACK_BUTTON_X + FEEDBACK_BUTTON_WIDTH + SPACER + 20; // non-client-area hack adjustment
const int WINDOW_HEIGHT = 200;

const int INFO_LINK_WIDTH = TextWidth(INFO_LINK_PROMPT);
const int INFO_LINK_HEIGHT = TextHeight();
const int INFO_LINK_X = 0 + (WINDOW_WIDTH - INFO_LINK_WIDTH)/2;
const int INFO_LINK_Y = WINDOW_HEIGHT - INFO_LINK_HEIGHT;

const int LOG_LIST_BOX_X = 0;
const int LOG_LIST_BOX_Y = SPLIT_TUNNEL_Y + SPLIT_TUNNEL_HEIGHT + SPACER;
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
HWND g_hSplitTunnelCheckBox = NULL;
HWND g_hLogListBox = NULL;
HWND g_hInfoLinkStatic = NULL;
HWND g_hInfoLinkTooltip = NULL;
HFONT g_hDefaultFont = NULL;
HFONT g_hUnderlineFont = NULL;
bool g_bShowEmail = false;
HWND g_hFeedbackButton = NULL;
HIMAGELIST g_hFeedbackButtonImageList = NULL;
const int FEEDBACK_BUTTON_ICON_COUNT = 2;
HICON g_hFeedbackButtonIcons[FEEDBACK_BUTTON_ICON_COUNT];


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


void SubclassHyperlink(HWND hWnd);

void CreateControls(HWND hWndParent)
{
    g_hDefaultFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    LOGFONT logfont;
    GetObject(g_hDefaultFont, sizeof(logfont), &logfont);
    logfont.lfUnderline = TRUE;
    g_hUnderlineFont = CreateFontIndirect(&logfont);

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

    SubclassHyperlink(g_hBannerStatic);

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

        SendMessage(g_hTransportRadioButtons[i], WM_SETFONT, (WPARAM)g_hDefaultFont, NULL);

        if (i == 0)
        {
            SendMessage(
                g_hTransportRadioButtons[i],
                BM_SETCHECK,
                BST_CHECKED,
                0);
        }
    }

    // Split Tunnel Check Box

    g_hSplitTunnelCheckBox = CreateWindow(
        L"Button",
        splitTunnelPrompt,
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        SPLIT_TUNNEL_X,
        SPLIT_TUNNEL_Y,
        SPLIT_TUNNEL_WIDTH,
        SPLIT_TUNNEL_HEIGHT,
        hWndParent,
        (HMENU)IDC_SPLIT_TUNNEL_CHECKBOX,
        g_hInst,
        NULL);
    
    SendMessage(g_hSplitTunnelCheckBox, WM_SETFONT, (WPARAM)g_hDefaultFont, NULL);

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
    SendMessage(g_hLogListBox, WM_SETFONT, (WPARAM)g_hDefaultFont, NULL);

    // Info Link

    // Hyperlink-like static control implementation adapted from:
    // http://www.olivierlanglois.net/hyperlinkdemo.htm

    g_hInfoLinkStatic = CreateWindow(
        L"Static",
        INFO_LINK_PROMPT,
        WS_CHILD|WS_VISIBLE|SS_NOTIFY,
        INFO_LINK_X,
        INFO_LINK_Y,
        INFO_LINK_WIDTH,
        INFO_LINK_HEIGHT,
        hWndParent,
        (HMENU)IDC_INFO_LINK_STATIC,
        g_hInst,
        NULL);
    SendMessage(g_hInfoLinkStatic, WM_SETFONT, (WPARAM)g_hDefaultFont, NULL);

    g_hInfoLinkTooltip = CreateWindowEx(
        NULL,
        TOOLTIPS_CLASS,
        NULL,
        WS_POPUP|TTS_ALWAYSTIP|TTS_BALLOON,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        hWndParent,
        NULL, 
        g_hInst,
        NULL);
    
    TOOLINFO toolInfo = {0};
    toolInfo.cbSize = sizeof(toolInfo);
    toolInfo.hwnd = hWndParent;
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.uId = (UINT_PTR)g_hInfoLinkStatic;
    toolInfo.lpszText = (TCHAR*)INFO_LINK_URL;
    SendMessage(g_hInfoLinkTooltip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);

    SubclassHyperlink(g_hInfoLinkStatic);

    // Feedback Button

    g_hFeedbackButton = CreateWindow(
        L"Button",
        L"",
        WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_ICON,
        FEEDBACK_BUTTON_X,
        FEEDBACK_BUTTON_Y,
        FEEDBACK_BUTTON_WIDTH,
        FEEDBACK_BUTTON_HEIGHT,
        hWndParent,
        (HMENU)IDC_FEEDBACK_BUTTON,
        g_hInst,
        NULL);

    g_hFeedbackButtonImageList = ImageList_LoadImage(
        g_hInst,
        MAKEINTRESOURCE(IDB_FEEDBACK_BUTTON_IMAGES),
        FEEDBACK_BUTTON_IMAGE_WIDTH,
        0,
        CLR_DEFAULT,
        IMAGE_BITMAP,
        LR_CREATEDIBSECTION);

    assert(FEEDBACK_BUTTON_ICON_COUNT == ImageList_GetImageCount(g_hFeedbackButtonImageList));

    for (int i = 0; i < FEEDBACK_BUTTON_ICON_COUNT; i++)
    {
        g_hFeedbackButtonIcons[i] = ImageList_GetIcon(
            g_hFeedbackButtonImageList,
            i,
            ILD_NORMAL);
    }

    SendMessage(
        g_hFeedbackButton,
        BM_SETIMAGE,
        IMAGE_ICON,
        (LPARAM)g_hFeedbackButtonIcons[0]);
}


const TCHAR* HYPERLINK_ORIGINAL_WINDOWS_PROCEDURE = _T("Original Static Control Windows Procedure");

LRESULT CALLBACK HyperlinkProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_MOUSEMOVE:
        if (GetCapture() != hWnd)
        {
            SendMessage(hWnd, WM_SETFONT, (WPARAM)g_hUnderlineFont, FALSE);
            InvalidateRect(hWnd, NULL, FALSE);
            SetCapture(hWnd);
        }
        else
        {
            RECT rect;
            GetWindowRect(hWnd, &rect);
            POINT point = {LOWORD(lParam), HIWORD(lParam)};
            ClientToScreen(hWnd, &point);
            if (!PtInRect(&rect, point))
            {
                SendMessage(hWnd, WM_SETFONT, (WPARAM)g_hDefaultFont, FALSE);
                InvalidateRect(hWnd, NULL, FALSE);
                ReleaseCapture();
            }
        }
        break;

    case WM_CAPTURECHANGED:
        SendMessage(hWnd, WM_SETFONT, (WPARAM)g_hDefaultFont, FALSE);
        InvalidateRect(hWnd, NULL, FALSE);
        break;

    case WM_SETCURSOR:
        SetCursor(LoadCursor(0, IDC_HAND));
        return TRUE;
    }

    WNDPROC proc = (WNDPROC)GetProp(hWnd, HYPERLINK_ORIGINAL_WINDOWS_PROCEDURE);
    return CallWindowProc(proc, hWnd, message, wParam, lParam);
}


void SubclassHyperlink(HWND hWnd)
{
    // NOTE: link color is handled in WM_CTLCOLORSTATIC in
    // the parent window function without subclassing

    SetProp(
        hWnd,
        HYPERLINK_ORIGINAL_WINDOWS_PROCEDURE,
        (HANDLE)GetWindowLong(hWnd, GWL_WNDPROC));
    SetWindowLong(
        hWnd,
        GWL_WNDPROC,
        (LONG)HyperlinkProc);
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
    time_t timeUntilEmail = 120;

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


void StoreSelectedTransport(void)
{
    string selectedTransport = TStringToNarrow(GetSelectedTransport());
    if (selectedTransport.length() > 0)
    {
        WriteRegistryStringValue(LOCAL_SETTINGS_REGISTRY_VALUE_TRANSPORT, selectedTransport);
    }
}


void EnableSplitTunnelForSelectedTransport();

void RestoreSelectedTransport(void)
{
    string selectedTransport;
    if (!ReadRegistryStringValue(LOCAL_SETTINGS_REGISTRY_VALUE_TRANSPORT, selectedTransport))
    {
        return;
    }

    int matchIndex = -1;
    for (int i = 0; i < transportOptionCount; i++)
    {
        if (selectedTransport == TStringToNarrow(transportOptions[i]))
        {
            matchIndex = i;
            break;
        }
    }

    // First check that it's a valid transport identifier
    if (-1 == matchIndex)
    {
        return;
    }

    for (int i = 0; i < transportOptionCount; i++)
    {
        SendMessage(
            g_hTransportRadioButtons[i],
            BM_SETCHECK,
            (i == matchIndex) ? BST_CHECKED : BST_UNCHECKED,
            0);
    }    

    EnableSplitTunnelForSelectedTransport();
}


void EnableSplitTunnelForSelectedTransport()
{
    // Split tunnel isn't implemented for VPN

    if (_T("VPN") == GetSelectedTransport())
    {
        ShowWindow(g_hSplitTunnelCheckBox, FALSE);
        SendMessage(g_hSplitTunnelCheckBox, BM_SETCHECK, BST_UNCHECKED, 0);
    }
    else
    {
        ShowWindow(g_hSplitTunnelCheckBox, TRUE);
    }
}


bool GetSplitTunnel()
{
    return (BST_CHECKED == SendMessage(g_hSplitTunnelCheckBox, BM_GETCHECK, 0, 0)) ? 1 : 0;
}


void StoreSplitTunnel()
{
    WriteRegistryDwordValue(LOCAL_SETTINGS_REGISTRY_VALUE_SPLIT_TUNNEL, GetSplitTunnel() ? 1 : 0);
}


void RestoreSplitTunnel()
{
    DWORD splitTunnel = 0;
    if (!ReadRegistryDwordValue(LOCAL_SETTINGS_REGISTRY_VALUE_SPLIT_TUNNEL, splitTunnel))
    {
        return;
    }

    SendMessage(
        g_hSplitTunnelCheckBox,
        BM_SETCHECK,
        splitTunnel ? BST_CHECKED : BST_UNCHECKED,
        0);
}


//==== my_print (logging) =====================================================

vector<MessageHistoryEntry> g_messageHistory;
HANDLE g_messageHistoryMutex = CreateMutex(NULL, FALSE, 0);

void GetMessageHistory(vector<MessageHistoryEntry>& history)
{
    AutoMUTEX mutex(g_messageHistoryMutex);
    history = g_messageHistory;
}

void AddMessageEntryToHistory(
        LogSensitivity sensitivity, 
        bool bDebugMessage, 
        const TCHAR* formatString,
        const TCHAR* finalString)
{
    AutoMUTEX mutex(g_messageHistoryMutex);

    const TCHAR* historicalMessage = NULL;
    if (sensitivity == NOT_SENSITIVE)
    {
        historicalMessage = finalString;
    }
    else if (sensitivity == SENSITIVE_FORMAT_ARGS)
    {
        historicalMessage = formatString;
    }
    else // SENSITIVE_LOG
    {
        historicalMessage = NULL;
    }

    if (historicalMessage != NULL)
    {
        MessageHistoryEntry entry;
        entry.message = historicalMessage;
        entry.timestamp = GetISO8601DatetimeString();
        entry.debug = bDebugMessage;
        g_messageHistory.push_back(entry);
    }
}


#ifdef _DEBUG
bool g_bShowDebugMessages = true;
#else
bool g_bShowDebugMessages = false;
#endif

void my_print(LogSensitivity sensitivity, bool bDebugMessage, const TCHAR* format, ...)
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

    AddMessageEntryToHistory(sensitivity, bDebugMessage, format, buffer);

    if (!bDebugMessage || g_bShowDebugMessages)
    {
        // NOTE:
        // Main window handles displaying the message. This avoids
        // deadlocks with SendMessage. Main window will deallocate
        // buffer.

        PostMessage(g_hWnd, WM_PSIPHON_MY_PRINT, NULL, (LPARAM)buffer);
    }
}

void my_print(LogSensitivity sensitivity, bool bDebugMessage, const string& message)
{
    my_print(sensitivity, bDebugMessage, NarrowToTString(message).c_str());
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

    DoStartupDiagnosticCollection();

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
    // Don't allow multiple instances of this application to run
    if (g_singleInstanceObject.IsAnotherInstanceRunning())
    {
        HWND otherWindow = FindWindow(g_szWindowClass, g_szTitle);
        if (otherWindow)
        {
            SetForegroundWindow(otherWindow);
            ShowWindow(otherWindow, SW_SHOW);
        }
        return FALSE;
    }

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
    LRESULT result;

    switch (message)
    {
    case WM_CREATE:
        CreateControls(hWnd);

        // We shouldn't try to start connecting while processing the WM_CREATE
        // message because otherwise messages logged by called functions will be lost.
        PostMessage(hWnd, WM_PSIPHON_CREATED, 0, 0);

        break;

    case WM_PSIPHON_CREATED:
        // Display client version number 

        my_print(NOT_SENSITIVE, false, (tstring(_T("Client Version: ")) + NarrowToTString(CLIENT_VERSION)).c_str());

        // NOTE: we leave the connection animation timer running even after fully connected
        // when the icon no longer animates -- since the timer handler also updates the UI
        // when unexpectedly disconnected.
        SetTimer(hWnd, IDT_BUTTON_ANIMATION, 250, NULL);

        // If there's a transport preference setting, restore it

        RestoreSelectedTransport();
        RestoreSplitTunnel();

        // Start a connection on the selected transport

        g_lastTransportSelection = GetSelectedTransport();
        g_connectionManager.Toggle(g_lastTransportSelection, GetSplitTunnel());

        // Optimize the server list

        g_serverListReorder.Start(&g_connectionManager.GetServerList());

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
                my_print(NOT_SENSITIVE, false, _T("Show debug messages: %s"), g_bShowDebugMessages ? _T("Yes") : _T("No"));
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
            my_print(NOT_SENSITIVE, true, _T("%s: Button pressed, Toggle called"), __TFUNCTION__);

            // See comment below about Stop() blocking the UI
            SetCursor(LoadCursor(0, IDC_WAIT));

            g_connectionManager.Toggle(GetSelectedTransport(), GetSplitTunnel());
        }

        // Transport radio button clicked

        else if (lParam != 0
                 && wmId >= IDC_TRANSPORT_OPTION_RADIO_FIRST && wmId <= IDC_TRANSPORT_OPTION_RADIO_LAST
                 && wmEvent == BN_CLICKED)
        {
            // Store the selection for next app run
            StoreSelectedTransport();

            tstring newTransportSelection = GetSelectedTransport();

            EnableSplitTunnelForSelectedTransport();

            RestoreSplitTunnel();

            if (newTransportSelection != g_lastTransportSelection)
            {
                // Restart with the new transport immediately

                // Show a Wait cursor since ConnectionManager::Stop() (called by Start) can
                // take a few seconds to complete, which blocks the radio button redrawing
                // animation. WM_SETCURSOR will reset the cursor automatically.
                SetCursor(LoadCursor(0, IDC_WAIT));

                g_connectionManager.Start(newTransportSelection, GetSplitTunnel());

                g_lastTransportSelection = newTransportSelection;
            }
        }
        
        // Banner clicked
        
        else if (lParam == (LPARAM)g_hBannerStatic && wmEvent == STN_CLICKED)
        {
            // If connected, sponsor open home pages, or info link if
            // no sponsor pages. If not connected, open info link.

            int state = g_connectionManager.GetState();
            if (CONNECTION_MANAGER_STATE_CONNECTED == state)
            {
                g_connectionManager.OpenHomePages(INFO_LINK_URL);
            }
            else
            {
                OpenBrowser(INFO_LINK_URL);
            }
        }

        // Split tunnel checkbox clicked
        
        else if (lParam == (LPARAM)g_hSplitTunnelCheckBox && wmEvent == STN_CLICKED)
        {
            // Store the selection for next app run
            StoreSplitTunnel();

            if (GetSplitTunnel())
            {
                g_connectionManager.StartSplitTunnel();
            }
            else
            {
                g_connectionManager.StopSplitTunnel();
            }
        }

        // Info link clicked
        
        else if (lParam == (LPARAM)g_hInfoLinkStatic && wmEvent == STN_CLICKED)
        {
            // Info link static control was clicked, so open Psiphon 3 page
            // NOTE: Info link may be opened when not tunneled
            
            OpenBrowser(INFO_LINK_URL);
        }

        // Feedback button clicked

        else if (lParam == (LPARAM)g_hFeedbackButton && wmEvent == BN_CLICKED)
        {
            my_print(NOT_SENSITIVE, true, _T("%s: Button pressed, Feedback called"), __TFUNCTION__);

            CryptoPP::AutoSeededRandomPool rng;
            const size_t randBytesLen = 8;
            byte randBytes[randBytesLen];
            rng.GenerateBlock(randBytes, randBytesLen);
            
            tstring args = _T("{\"diagnosticInfoID\": \"") + NarrowToTString(Hexlify(randBytes, randBytesLen)) + _T("\"}");
            
            tstring feedbackResult;
            if (ShowHTMLDlg(
                    hWnd, 
                    _T("FEEDBACK_HTML_RESOURCE"), 
                    GetLocaleName().c_str(),
                    args.c_str(),
                    feedbackResult) == 1)
            {
                my_print(NOT_SENSITIVE, false, _T("Sending feedback..."));

                g_connectionManager.SendFeedback(feedbackResult.c_str());

                SendMessage(
                    g_hFeedbackButton,
                    BM_SETIMAGE,
                    IMAGE_ICON,
                    (LPARAM)g_hFeedbackButtonIcons[1]);
                EnableWindow(g_hFeedbackButton, FALSE);
            }
            // else error or user cancelled
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

    case WM_PSIPHON_FEEDBACK_SUCCESS:
        SendMessage(
            g_hFeedbackButton,
            BM_SETIMAGE,
            IMAGE_ICON,
            (LPARAM)g_hFeedbackButtonIcons[1]);
        EnableWindow(g_hFeedbackButton, FALSE);
        my_print(NOT_SENSITIVE, false, _T("Feedback sent. Thank you!"));
        break;

    case WM_PSIPHON_FEEDBACK_FAILED:
        SendMessage(
            g_hFeedbackButton,
            BM_SETIMAGE,
            IMAGE_ICON,
            (LPARAM)g_hFeedbackButtonIcons[0]);
        EnableWindow(g_hFeedbackButton, TRUE);
        my_print(NOT_SENSITIVE, false, _T("Failed to send feedback."));
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;

    case WM_CTLCOLORSTATIC:
        result = DefWindowProc(hWnd, message, wParam, lParam);
        // Set color for info link static control
        if ((HWND)lParam == g_hInfoLinkStatic)
        {
            SetTextColor((HDC)wParam, RGB(0, 0, 192));
        }
        return result;

    case WM_DESTROY:
        // Stop VPN if running
        // The order of these calls is important. The serverListReorder
        // requires connectionManager to stay up while it shuts down.
        g_serverListReorder.Stop();
        g_connectionManager.Stop(STOP_REASON_EXIT);
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
