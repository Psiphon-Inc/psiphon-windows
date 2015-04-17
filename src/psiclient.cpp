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
#include "logging.h"
#include <mCtrl/html.h>
#include "connectionmanager.h"
#include "embeddedvalues.h"
#include "transport.h"
#include "config.h"
#include "usersettings.h"
#include "utilities.h"
#include "webbrowser.h"
#include "limitsingleinstance.h"
#include "htmldlg.h"
#include "stopsignal.h"
#include "diagnostic_info.h"
#include "systemproxysettings.h"

//==== Globals ================================================================

#define MAX_LOADSTRING 100

HINSTANCE g_hInst;
TCHAR g_szTitle[MAX_LOADSTRING];
TCHAR g_szWindowClass[MAX_LOADSTRING];

HWND g_hWnd = NULL;
ConnectionManager g_connectionManager;

LimitSingleInstance g_singleInstanceObject(TEXT("Global\\{B88F6262-9CC8-44EF-887D-FB77DC89BB8C}"));

static HWND g_hHtmlCtrl = NULL;
static bool g_htmlUiReady = false;
// The HTML control has a bad habit of sending messages after we've posted WM_QUIT,
// which leads to a crash on exit.
static bool g_htmlUiFinished = false;

//==== Controls ================================================================

static void OnResize(HWND hWnd, UINT uWidth, UINT uHeight)
{
    SetWindowPos(g_hHtmlCtrl, NULL, 0, 0, uWidth, uHeight, SWP_NOZORDER);
}

void OnCreate(HWND hWndParent)
{
    Json::Value initJSON, settingsJSON;
    Settings::ToJson(settingsJSON);
    initJSON["Settings"] = settingsJSON;
    initJSON["Cookies"] = Settings::GetCookies();
    initJSON["Config"] = Json::Value();
    initJSON["Config"]["Language"] = TStringToNarrow(GetLocaleName());
    initJSON["Config"]["Banner"] = "banner.png";
    initJSON["Config"]["Version"] = CLIENT_VERSION;
    initJSON["Config"]["InfoURL"] = TStringToNarrow(INFO_LINK_URL);
#ifdef _DEBUG
    initJSON["Config"]["Debug"] = true;
#else
    initJSON["Config"]["Debug"] = false;
#endif

    Json::FastWriter jsonWriter;
    tstring initJsonString = NarrowToTString(jsonWriter.write(initJSON));

    tstring url = ResourceToUrl(_T("main.html"), NULL, initJsonString.c_str());

    /* Create the html control */
    g_hHtmlCtrl = CreateWindow(
        MC_WC_HTML,
        url.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        MC_HS_NOCONTEXTMENU |   // don't show context menu
        MC_HS_NOTIFYNAV,        // notify owner window on navigation attempts
        0, 0, 0, 0,
        hWndParent,
        (HMENU)IDC_HTML_CTRL,
        g_hInst,
        NULL);
}


//==== HTML UI helpers ========================================================

// Many of these helpers (particularly the ones that don't need an immediate
// response from the page script) come in pairs: one function to receive the
// arguments, create a buffer, and post a message; and one function to receive
// the posted message and actually do the work.
// We do this so that we won't end up deadlocked between message handling and
// background stuff. For example, the Stop button in the HTML will block the
// page script until the AppLink is processed; but if ConnectionManager.Stop()
// is called directly, then it will wait for the connection thread to die, but
// that thread calls ConnectionManager.SetState(), which calls HtmlUI_SetState(),
// which tries to talk to the page script, but it can't, because the page script
// is blocked!
// So, we're going to PostMessages to ourself whenever possible.

#define WM_PSIPHON_HTMLUI_BEFORENAVIGATE    WM_USER + 200
#define WM_PSIPHON_HTMLUI_SETSTATE          WM_USER + 201
#define WM_PSIPHON_HTMLUI_ADDMESSAGE        WM_USER + 202
#define WM_PSIPHON_HTMLUI_ADDNOTICE         WM_USER + 203

static void HtmlUI_AddMessage(int priority, LPCTSTR message)
{
    Json::Value json;
    json["priority"] = priority;
    json["message"] = WStringToUTF8(message);
    Json::FastWriter jsonWriter;
    wstring wJson = UTF8ToWString(jsonWriter.write(json).c_str());

    size_t bufLen = wJson.length() + 1;
    wchar_t* buf = new wchar_t[bufLen];
    wcsncpy_s(buf, bufLen, wJson.c_str(), bufLen);
    buf[bufLen - 1] = L'\0';
    PostMessage(g_hWnd, WM_PSIPHON_HTMLUI_ADDMESSAGE, (WPARAM)buf, 0);
}

static void HtmlUI_AddMessageHandler(LPCWSTR json)
{
    if (!g_htmlUiReady)
    {
        delete[] json;
        return;
    }

    MC_HMCALLSCRIPTFUNC argStruct = { 0 };
    argStruct.cbSize = sizeof(MC_HMCALLSCRIPTFUNC);
    argStruct.cArgs = 1;
    argStruct.pszArg1 = json;
    (void)SendMessage(
        g_hHtmlCtrl, MC_HM_CALLSCRIPTFUNC,
        (WPARAM)_T("HtmlCtrlInterface_AddMessage"), (LPARAM)&argStruct);
    delete[] json;
}

static void HtmlUI_SetState(const wstring& json)
{
    size_t bufLen = json.length() + 1;
    wchar_t* buf = new wchar_t[bufLen];
    wcsncpy_s(buf, bufLen, json.c_str(), bufLen);
    buf[bufLen - 1] = L'\0';
    PostMessage(g_hWnd, WM_PSIPHON_HTMLUI_SETSTATE, (WPARAM)buf, 0);
}

static void HtmlUI_SetStateHandler(LPCWSTR json)
{
    if (!g_htmlUiReady)
    {
        delete[] json;
        return;
    }

    MC_HMCALLSCRIPTFUNC argStruct = { 0 };
    argStruct.cbSize = sizeof(MC_HMCALLSCRIPTFUNC);
    argStruct.cArgs = 1;
    argStruct.pszArg1 = json;
    (void)SendMessage(
        g_hHtmlCtrl, MC_HM_CALLSCRIPTFUNC,
        (WPARAM)_T("HtmlCtrlInterface_SetState"), (LPARAM)&argStruct);
    delete[] json;
}

static void HtmlUI_AddNotice(const string& noticeJSON)
{
    wstring wJson = UTF8ToWString(noticeJSON.c_str());

    size_t bufLen = wJson.length() + 1;
    wchar_t* buf = new wchar_t[bufLen];
    wcsncpy_s(buf, bufLen, wJson.c_str(), bufLen);
    buf[bufLen - 1] = L'\0';
    PostMessage(g_hWnd, WM_PSIPHON_HTMLUI_ADDNOTICE, (WPARAM)buf, 0);
}

static void HtmlUI_AddNoticeHandler(LPCWSTR json)
{
    if (!g_htmlUiReady)
    {
        delete[] json;
        return;
    }

    MC_HMCALLSCRIPTFUNC argStruct = { 0 };
    argStruct.cbSize = sizeof(MC_HMCALLSCRIPTFUNC);
    argStruct.cArgs = 1;
    argStruct.pszArg1 = json;
    (void)SendMessage(
        g_hHtmlCtrl, MC_HM_CALLSCRIPTFUNC,
        (WPARAM)_T("HtmlCtrlInterface_AddNotice"), (LPARAM)&argStruct);
    delete[] json;
}

static void HtmlUI_BeforeNavigate(MC_NMHTMLURL* nmHtmlUrl)
{
    size_t bufLen = _tcslen(nmHtmlUrl->pszUrl) + 1;
    TCHAR* buf = new TCHAR[bufLen];
    _tcsncpy_s(buf, bufLen, nmHtmlUrl->pszUrl, bufLen);
    buf[bufLen - 1] = _T('\0');
    PostMessage(g_hWnd, WM_PSIPHON_HTMLUI_BEFORENAVIGATE, (WPARAM)buf, 0);
}

#define PSIPHON_LINK_PREFIX     _T("psi:")
static void HtmlUI_BeforeNavigateHandler(LPCTSTR url)
{
    // NOTE: Incoming query parameters will be URI-encoded

    const LPCTSTR appReady = PSIPHON_LINK_PREFIX _T("ready");
    const LPCTSTR appStart = PSIPHON_LINK_PREFIX _T("start");
    const LPCTSTR appStop = PSIPHON_LINK_PREFIX _T("stop");
    const LPCTSTR appUpdateSettings = PSIPHON_LINK_PREFIX _T("updatesettings?");
    const size_t appUpdateSettingsLen = _tcslen(appUpdateSettings);
    const LPCTSTR appSendFeedback = PSIPHON_LINK_PREFIX _T("sendfeedback?");
    const size_t appSendFeedbackLen = _tcslen(appSendFeedback);
    const LPCTSTR appSetCookies = PSIPHON_LINK_PREFIX _T("setcookies?");
    const size_t appSetCookiesLen = _tcslen(appSetCookies);
    const LPCTSTR appBannerClick = PSIPHON_LINK_PREFIX _T("bannerclick");

    if (_tcscmp(url, appReady) == 0)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Ready requested"), __TFUNCTION__);
        g_htmlUiReady = true;
        PostMessage(g_hWnd, WM_PSIPHON_CREATED, 0, 0);
    }
    else if (_tcscmp(url, appStart) == 0)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Start requested"), __TFUNCTION__);
        g_connectionManager.Start();
    }
    else if (_tcscmp(url, appStop) == 0)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Stop requested"), __TFUNCTION__);
        g_connectionManager.Stop(STOP_REASON_USER_DISCONNECT);
    }
    else if (_tcsncmp(url, appUpdateSettings, appUpdateSettingsLen) == 0
        && _tcslen(url) > appUpdateSettingsLen)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Update settings requested"), __TFUNCTION__);
        tstring uriEncoded(url + appUpdateSettingsLen);
        string stringJSON = UriDecode(TStringToNarrow(uriEncoded));
        bool settingsChanged = false;
        if (Settings::FromJson(stringJSON, settingsChanged) && settingsChanged
            && (g_connectionManager.GetState() == CONNECTION_MANAGER_STATE_CONNECTED
            || g_connectionManager.GetState() == CONNECTION_MANAGER_STATE_STARTING))
        {
            my_print(NOT_SENSITIVE, false, _T("Settings change detected. Reconnecting."));
            g_connectionManager.Stop(STOP_REASON_USER_DISCONNECT);
            g_connectionManager.Start();
        }
    }
    else if (_tcsncmp(url, appSendFeedback, appSendFeedbackLen) == 0
        && _tcslen(url) > appSendFeedbackLen)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Send feedback requested"), __TFUNCTION__);
        tstring uriEncoded(url + appSendFeedbackLen);
        string stringJSON = UriDecode(TStringToNarrow(uriEncoded));
        my_print(NOT_SENSITIVE, false, _T("Sending feedback..."));
        g_connectionManager.SendFeedback(NarrowToTString(stringJSON).c_str());
    }
    else if (_tcsncmp(url, appSetCookies, appSetCookiesLen) == 0
        && _tcslen(url) > appSetCookiesLen)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Set cookies requested"), __TFUNCTION__);
        tstring uriEncoded(url + appSetCookiesLen);
        string stringJSON = UriDecode(TStringToNarrow(uriEncoded));
        Settings::SetCookies(stringJSON);
    }
    else if (_tcscmp(url, appBannerClick) == 0)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Banner clicked"), __TFUNCTION__);
        // If connected, open sponsor home pages, or info link if
        // no sponsor pages. If not connected, open info link.
        if (CONNECTION_MANAGER_STATE_CONNECTED == g_connectionManager.GetState())
        {
            g_connectionManager.OpenHomePages(INFO_LINK_URL, false);
        }
        else
        {
            OpenBrowser(INFO_LINK_URL);
        }
    }
    else {
        // Not one of our links. Open it in an external browser.
        OpenBrowser(url);
    }

    delete[] url;
}

//==== Exported functions ========================================================

void UI_SetStateStopped()
{
    Json::Value json;
    json["state"] = "stopped";
    Json::FastWriter jsonWriter;
    wstring wJson = UTF8ToWString(jsonWriter.write(json).c_str());
    HtmlUI_SetState(wJson);
}

void UI_SetStateStopping()
{
    Json::Value json;
    json["state"] = "stopping";
    Json::FastWriter jsonWriter;
    wstring wJson = UTF8ToWString(jsonWriter.write(json).c_str());
    HtmlUI_SetState(wJson);
}

void UI_SetStateStarting(const tstring& transportProtocolName)
{
    Json::Value json;
    json["state"] = "starting";
    json["transport"] = WStringToUTF8(transportProtocolName.c_str());
    Json::FastWriter jsonWriter;
    wstring wJson = UTF8ToWString(jsonWriter.write(json).c_str());
    HtmlUI_SetState(wJson);
}

void UI_SetStateConnected(const tstring& transportProtocolName, int socksPort, int httpPort)
{
    Json::Value json;
    json["state"] = "connected";
    json["transport"] = WStringToUTF8(transportProtocolName.c_str());
    json["socksPort"] = socksPort;
    json["socksPortAuto"] = Settings::LocalSocksProxyPort() == 0;
    json["httpPort"] = httpPort;
    json["httpPortAuto"] = Settings::LocalHttpProxyPort() == 0;
    Json::FastWriter jsonWriter;
    wstring wJson = UTF8ToWString(jsonWriter.write(json).c_str());
    HtmlUI_SetState(wJson);
}

void UI_Notice(const string& noticeJSON)
{
    HtmlUI_AddNotice(noticeJSON);
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

    /* Register mCtrl and its HTML control. */
    mc_StaticLibInitialize();
    mcHtml_Initialize();

    // Perform application initialization

    if (!InitInstance(hInstance, nCmdShow))
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
            if (msg.message == (WM_APP + 2) && g_htmlUiFinished)
                continue;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    mcHtml_Terminate();
    mc_StaticLibTerminate();

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

static LRESULT HandleNotify(HWND hWnd, NMHDR* hdr)
{
    if (hdr->idFrom == IDC_HTML_CTRL)
    {
        if (hdr->code == MC_HN_BEFORENAVIGATE)
        {
            MC_NMHTMLURL* nmHtmlUrl = (MC_NMHTMLURL*)hdr;
            // We should not interfere with the initial page load
            static bool s_firstNav = true;
            if (s_firstNav) {
                s_firstNav = false;
                return 0;
            }

            HtmlUI_BeforeNavigate(nmHtmlUrl);
            return -1; // Prevent navigation
        }
        else if (hdr->code == MC_HN_NEWWINDOW)
        {
            MC_NMHTMLURL* nmHtmlUrl = (MC_NMHTMLURL*)hdr;
            // Prevent new window from opening
            return 0;
        }
        else if (hdr->code == MC_HN_HTTPERROR)
        {
            MC_NMHTTPERROR* nmHttpError = (MC_NMHTTPERROR*)hdr;
            assert(false);
            // Prevent HTTP error from being shown.
            return 0;
        }
    }

    return 0;
}

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

    g_hWnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        g_szWindowClass,
        g_szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        780, 580,
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
        // Display client version number
        my_print(NOT_SENSITIVE, false, (tstring(_T("Client Version: ")) + NarrowToTString(CLIENT_VERSION)).c_str());

        // Content is loaded, so show the window.
        ShowWindow(g_hWnd, SW_SHOW);

        // Start a connection
        if (!Settings::SkipAutoConnect())
        {
            g_connectionManager.Toggle();
        }
        break;

    case WM_PSIPHON_HTMLUI_BEFORENAVIGATE:
        HtmlUI_BeforeNavigateHandler((LPCTSTR)wParam);
        break;
    case WM_PSIPHON_HTMLUI_SETSTATE:
        HtmlUI_SetStateHandler((LPCWSTR)wParam);
        break;
    case WM_PSIPHON_HTMLUI_ADDMESSAGE:
        HtmlUI_AddMessageHandler((LPCWSTR)wParam);
        break;
    case WM_PSIPHON_HTMLUI_ADDNOTICE:
        HtmlUI_AddNoticeHandler((LPCWSTR)wParam);
        break;

    case WM_SIZE:
        if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
        {
            OnResize(hWnd, LOWORD(lParam), HIWORD(lParam));
        }
        break;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 680;
        mmi->ptMinTrackSize.y = 410;
        break;
    }

    case WM_SETFOCUS:
        SetFocus(g_hHtmlCtrl);
        break;

    case WM_NOTIFY:
        return HandleNotify(hWnd, (NMHDR*)lParam);
        break;

    case WM_PSIPHON_MY_PRINT:
    {
        int priority = (int)wParam;
        TCHAR* message = (TCHAR*)lParam;
        HtmlUI_AddMessage(priority, message);
        OutputDebugString(message);
        OutputDebugString(L"\n");
        free(message);
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
        g_htmlUiFinished = true;
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
