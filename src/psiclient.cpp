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

 //==== Includes ===============================================================

#include "stdafx.h"

// This is for COM functions
# pragma comment(lib, "wbemuuid.lib")

#include "psiclient.h"
#include "psiclient_ui.h"
#include "psiclient_systray.h"
#include "logging.h"
#include "connectionmanager.h"
#include "transport.h"
#include "utilities.h"
#include "limitsingleinstance.h"
#include "diagnostic_info.h"
#include "systemproxysettings.h"
#include "embeddedvalues.h"
#include "usersettings.h"

//==== Globals ================================================================

#define MAX_LOADSTRING 100

HINSTANCE g_hInst;
std::tstring g_appTitle;
TCHAR g_szWindowClass[MAX_LOADSTRING];

HWND g_hWnd = NULL;
static float g_dpiScaling = 1.0;
static bool g_windowRestored = false;

ConnectionManager g_connectionManager;

LimitSingleInstance g_singleInstanceObject(TEXT("Global\\{B88F6262-9CC8-44EF-887D-FB77DC89BB8C}"));

// The HTML control has a bad habit of sending messages after we've posted WM_QUIT,
// which leads to a crash on exit.
static bool g_uiIsShutDown = false;
bool IsUIShutDown() {
    return g_uiIsShutDown;
}


//==== Forward declarations ====================================================

void ForegroundWindow(HWND hwnd);


//==== Controls ================================================================

static void OnResize(HWND hWnd, UINT uWidth, UINT uHeight)
{
    SetWindowPos(GetHTMLControl(), NULL, 0, 0, uWidth, uHeight, SWP_NOZORDER);
}

void OnCreate(HWND hWndParent)
{
    CreateHTMLControl(hWndParent, g_dpiScaling);
}


//==== Win32 boilerplate ======================================================

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE hInstance, LPTSTR lpCmdLine, int nShowCmd);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPTSTR lpCmdLine,
    _In_ int nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    TCHAR szAppTitle[MAX_LOADSTRING];
    LoadString(hInstance, IDS_APP_TITLE, szAppTitle, MAX_LOADSTRING);
    g_appTitle = szAppTitle;
    LoadString(hInstance, IDC_PSICLIENT, g_szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    InitHTMLLib();

    // Perform application initialization

    if (!InitInstance(hInstance, lpCmdLine, nShowCmd))
    {
        return FALSE;
    }

    HACCEL hAccelTable;
    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PSICLIENT));

    // If this set of calls gets any longer, we may want to do something generic.
    DoStartupSystemProxyWork();
    DoStartupDiagnosticCollection();

    // Main message loop

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            // Bit of a dirty hack to prevent the HTML control code from crashing
            // on exit.
            // WM_APP+2 is the message used for MC_HN_STATUSTEXT. Sometimes it
            // arrives after the control is destroyed and will cause an app
            // crash if we let it through.
            if (msg.message == (WM_APP + 2) && IsUIShutDown())
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    SystrayIconCleanup();

    CleanupHTMLLib();

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex = { 0 };

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PSICLIENT));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = g_szWindowClass;
    wcex.hIconSm = NULL;

    return RegisterClassEx(&wcex);
}

//==== Main window functions ==================================================

#define WINDOW_X_START  900
#define WINDOW_Y_START  700
#define WINDOW_X_MIN    790
#define WINDOW_Y_MIN    700

static void SaveWindowPlacement()
{
    if (!g_hWnd)
    {
        return;
    }

    WINDOWPLACEMENT wp = { 0 };
    wp.length = sizeof(WINDOWPLACEMENT);
    if (!GetWindowPlacement(g_hWnd, &wp))
    {
        return;
    }

    Json::Value json;
    json["showCmd"] = wp.showCmd;
    json["rcNormalPosition.top"] = wp.rcNormalPosition.top;
    json["rcNormalPosition.bottom"] = wp.rcNormalPosition.bottom;
    json["rcNormalPosition.left"] = wp.rcNormalPosition.left;
    json["rcNormalPosition.right"] = wp.rcNormalPosition.right;
    Json::FastWriter jsonWriter;
    Settings::SetWindowPlacement(jsonWriter.write(json));
}

static void RestoreWindowPlacement()
{
    auto restoreExit = finally([] {
        ShowWindow(g_hWnd, SW_SHOW);
        g_windowRestored = true;
    });

    if (!g_hWnd)
    {
        return;
    }

    string windowPlacementJson = Settings::GetWindowPlacement();
    if (windowPlacementJson.empty())
    {
        // No stored placement
        return;
    }

    WINDOWPLACEMENT wp = { 0 };
    wp.length = sizeof(WINDOWPLACEMENT);

    Json::Value json;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(windowPlacementJson, json);
    if (!parsingSuccessful)
    {
        my_print(NOT_SENSITIVE, false, _T("Failed to parse previous window placement"));
        return;
    }

    try
    {
        if (!json.isMember("showCmd") ||
            !json.isMember("rcNormalPosition.top") ||
            !json.isMember("rcNormalPosition.bottom") ||
            !json.isMember("rcNormalPosition.left") ||
            !json.isMember("rcNormalPosition.right"))
        {
            // The stored values are invalid
            return;
        }

        wp.showCmd = json.get("showCmd", SW_SHOWNORMAL).asUInt();
        // Don't allow restoring minimized
        if (wp.showCmd == SW_SHOWMINIMIZED)
        {
            wp.showCmd = SW_SHOWNORMAL;
        }

        wp.rcNormalPosition.top = (LONG)json.get("rcNormalPosition.top", 0).asLargestInt();
        wp.rcNormalPosition.bottom = (LONG)json.get(
            "rcNormalPosition.bottom",
            (LONG)ceil(WINDOW_Y_START * g_dpiScaling)).asLargestInt();
        wp.rcNormalPosition.left = (LONG)json.get("rcNormalPosition.left", 0).asLargestInt();
        wp.rcNormalPosition.right = (LONG)json.get(
            "rcNormalPosition.right",
            (LONG)ceil(WINDOW_X_START * g_dpiScaling)).asLargestInt();
    }
    catch (exception& e)
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d: JSON parse exception: %S"), __TFUNCTION__, __LINE__, e.what());
        return;
    }

    SetWindowPlacement(g_hWnd, &wp);
}

static LRESULT HandleNotify(HWND hWnd, NMHDR* hdr)
{
    if (hdr->idFrom == IDC_HTML_CTRL)
    {
        return HandleNotifyHTMLControl(hWnd, hdr);
    }

    return 0;
}

BOOL InitInstance(HINSTANCE hInstance, LPTSTR lpCmdLine, int nShowCmd)
{
    // Don't allow multiple instances of this application to run
    if (g_singleInstanceObject.IsAnotherInstanceRunning())
    {
        HWND otherWindow = FindWindow(g_szWindowClass, g_appTitle.c_str());
        if (otherWindow)
        {
            ForegroundWindow(otherWindow);
            SendCommandLineToWnd(otherWindow, lpCmdLine);
        }
        return FALSE;
    }

    RegisterPsiphonProtocolHandler();
    ProcessCommandLine(lpCmdLine ? lpCmdLine : _T(""));

    g_hInst = hInstance;

    // This isn't supported for all OS versions.
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    if (GetDpiScalingForCurrentMonitor(g_hWnd, g_dpiScaling) != S_OK)
    {
        g_dpiScaling = 1.0;
    }
    my_print(NOT_SENSITIVE, true, _T("%s:%d: Initial DPI scaling: %f"), __TFUNCTION__, __LINE__, g_dpiScaling);

    g_hWnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        g_szWindowClass,
        g_appTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        (LONG)ceil(WINDOW_X_START * g_dpiScaling), (LONG)ceil(WINDOW_Y_START * g_dpiScaling),
        NULL, NULL, hInstance, NULL);

    // Don't show the window until the content loads.

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        OnCreate(hWnd);
        break;

    case WM_PSIPHON_CREATED:
    {
        // Display client version number
        my_print(NOT_SENSITIVE, false, (tstring(_T("Client Version: ")) + UTF8ToWString(CLIENT_VERSION)).c_str());

        if (!IsOSSupported())
        {
            // We're not showing the main app window, as it will probably be garbage
            // and the app should close momentarily.
            break;
        }

        // Content is loaded, so show the window.
        RestoreWindowPlacement();
        UpdateSystrayConnectedState();

        // Set initial state.
        UI_SetStateStopped();

        // Start a connection
        if (!Settings::SkipAutoConnect())
        {
            g_connectionManager.Toggle();
        }
        break;
    }

    case WM_DPICHANGED:
    {
        // This message is received when the window is moved between monitors
        // with different DPI settings. We need to scale our content.

        //auto x = LOWORD(wParam);
        auto y = HIWORD(wParam);
        auto rect = *reinterpret_cast<RECT*>(lParam);

        if (g_windowRestored)
        {
            SetWindowPos(
                hWnd, // no relative window
                NULL,
                rect.left,
                rect.top,
                rect.right - rect.left,
                rect.bottom - rect.top,
                SWP_NOACTIVATE | SWP_NOZORDER);
        }

        g_dpiScaling = ConvertDpiToScaling(y);

        my_print(NOT_SENSITIVE, true, _T("WM_DPICHANGED: %f"), g_dpiScaling);

        Json::Value dpiScalingJSON;
        dpiScalingJSON["dpiScaling"] = g_dpiScaling;

        Json::FastWriter jsonWriter;
        string strDpiScalingJSON = jsonWriter.write(dpiScalingJSON);
        UI_UpdateDpiScaling(strDpiScalingJSON);

        break;
    }

    case WM_COPYDATA:
    {
        // We only have one consumer of WM_COPYDATA messages
        ProcessCommandLineMessage(wParam, lParam);
        break;
    }

    case WM_PSIPHON_HTMLUI_BEFORENAVIGATE:
    case WM_PSIPHON_HTMLUI_SETSTATE:
    case WM_PSIPHON_HTMLUI_ADDLOG:
    case WM_PSIPHON_HTMLUI_ADDNOTICE:
    case WM_PSIPHON_HTMLUI_REFRESHSETTINGS:
    case WM_PSIPHON_HTMLUI_UPDATEDPISCALING:
    case WM_PSIPHON_HTMLUI_PSICASHMESSAGE:
    case WM_PSIPHON_HTMLUI_DEEPLINK:
        HTMLControlWndProc(message, wParam, lParam);
        break;

    case WM_SIZE:
        if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
        {
            OnResize(hWnd, LOWORD(lParam), HIWORD(lParam));
        }
        break;

    case WM_GETMINMAXINFO:

    {
        // This message is received when the system wants to know the minimum
        // window size (etc.) for this application. It needs to take scaling
        // into account.

        // It can happen that the we end up in this handler before WM_DPICHANGED
        // is received, but after the current monitor DPI is different. To avoid
        // any window size mistakes, we won't set the minimum size if that
        // mismatch is detected.

        float curMonDpiScaling = 1.0;
        GetDpiScalingForCurrentMonitor(g_hWnd, curMonDpiScaling);

        MINMAXINFO* mmi = (MINMAXINFO*)lParam;

        if (g_windowRestored && curMonDpiScaling == g_dpiScaling)
        {
            mmi->ptMinTrackSize.x = (LONG)ceil(WINDOW_X_MIN * g_dpiScaling);
            mmi->ptMinTrackSize.y = (LONG)ceil(WINDOW_Y_MIN * g_dpiScaling);
        }

        break;
    }

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE)
        {
            HandleMinimize();
        }
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_SETFOCUS:
        SetFocus(GetHTMLControl());
        break;

    case WM_NOTIFY:
        return HandleNotify(hWnd, (NMHDR*)lParam);

    case WM_PSIPHON_TRAY_CONNECTED_REMINDER_NOTIFY:
    case WM_PSIPHON_TRAY_ICON_NOTIFY:
        SystrayWndProc(message, wParam, lParam);
        break;

    case WM_PSIPHON_MY_PRINT:
    {
        int priority = (int)wParam;
        TCHAR* log = (TCHAR*)lParam;
        HtmlUI_AddLog(priority, log);
        auto timestamp = UTF8ToWString(psicash::datetime::DateTime::Now().ToISO8601() + ": ");
        OutputDebugString(timestamp.c_str());
        OutputDebugString(log);
        OutputDebugString(L"\n");
        free(log);
        break;
    }

    case WM_PSIPHON_FEEDBACK_SUCCESS:
        my_print(NOT_SENSITIVE, false, _T("Feedback sent. Thank you!"));
        break;

    case WM_PSIPHON_FEEDBACK_FAILED:
        my_print(NOT_SENSITIVE, false, _T("Failed to send feedback."));
        break;

    case WM_ENDSESSION:
        // Stop the tunnel -- particularly to ensure system proxy settings are reverted -- on OS shutdown
        // Note: due to the following bug, the system proxy settings revert may silently fail:
        // https://connect.microsoft.com/IE/feedback/details/838086/internet-explorer-10-11-wininet-api-drops-proxy-change-events-during-system-shutdown
    case WM_DESTROY:
        // Stop transport if running
        g_connectionManager.Stop(STOP_REASON_EXIT);
        g_uiIsShutDown = true;
        SaveWindowPlacement();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

/// Make the given window visible and foreground. If hwnd is NULL, the main window will be foregrounded.
void ForegroundWindow(HWND hwnd)
{
    if (!hwnd) {
        hwnd = g_hWnd;
    }

    // Un-minimize if necessary
    WINDOWPLACEMENT wp = { 0 };
    wp.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(hwnd, &wp) &&
        wp.showCmd == SW_SHOWMINIMIZED || wp.showCmd == SW_HIDE)
    {
        ShowWindow(hwnd, SW_RESTORE);
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
}
