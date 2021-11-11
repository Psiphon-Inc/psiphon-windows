/*
 * Copyright (c) 2021, Psiphon Inc.
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
#include "psiclient_ui.h"
#include "psiclient.h"
#include "psiclient_systray.h"
#include "htmldlg.h"
#include "usersettings.h"
#include "embeddedvalues.h"
#include "utilities.h"
#include "logging.h"
#include <mCtrl/html.h>
#include "webbrowser.h"
#include <algorithm>


// Set to TRUE to force the debug pane to be visible
#define DEBUG_UI    _DEBUG

static HWND g_hHtmlCtrl = NULL;
static bool g_htmlUiReady = false;
static wstring g_queuedDeeplink;
static bool g_psiCashInitializd = false;
static string g_uiLocale;


// Forward declarations
void InitPsiCash();
bool HandlePsiCashCommand(const string&);


//==== Initialization helpers ==================================================

void InitHTMLLib() {
    // OleInitialize is necessary to make HTML control clipboard functions (via context menu) work properly
    HRESULT hRes = OleInitialize(0);
    if (hRes != S_OK) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: OleInitialize failed: %d"), __TFUNCTION__, __LINE__, hRes);
        // Carry on and hope for the best
    }

    // Register mCtrl and its HTML control.
    mc_StaticLibInitialize();
    if (!mcHtml_Initialize()) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: mcHtml_Initialize failed"), __TFUNCTION__, __LINE__);
        // Carry on and hope for the best
    }
}

void CleanupHTMLLib() {
    mcHtml_Terminate();
    mc_StaticLibTerminate();
    OleUninitialize();
}

void CreateHTMLControl(HWND hWndParent, float dpiScaling) {
    Json::Value initJSON, settingsJSON;
    Settings::ToJson(settingsJSON);
    initJSON["Settings"] = settingsJSON;
    initJSON["Cookies"] = Settings::GetCookies();
    initJSON["Config"] = Json::Value();
    initJSON["Config"]["ClientVersion"] = CLIENT_VERSION;
    initJSON["Config"]["ClientBuild"] = GetBuildTimestamp();
    initJSON["Config"]["Language"] = WStringToUTF8(GetLocaleID());
    initJSON["Config"]["Banner"] = "banner.png";
    initJSON["Config"]["InfoURL"] = WStringToUTF8(INFO_LINK_URL);
    initJSON["Config"]["NewVersionEmail"] = GET_NEW_VERSION_EMAIL;
    initJSON["Config"]["NewVersionURL"] = GET_NEW_VERSION_URL;
    initJSON["Config"]["FaqURL"] = FAQ_URL;
    initJSON["Config"]["DataCollectionInfoURL"] = DATA_COLLECTION_INFO_URL;
    initJSON["Config"]["DpiScaling"] = dpiScaling;
#if DEBUG_UI
    initJSON["Config"]["Debug"] = true;
#else
    initJSON["Config"]["Debug"] = false;
#endif

    Json::FastWriter jsonWriter;
    tstring initJsonString = UTF8ToWString(jsonWriter.write(initJSON).c_str());
    tstring encodedJson = PercentEncode(initJsonString);

    tstring url = ResourceToUrl(_T("main.html"), NULL, encodedJson.c_str());

    g_hHtmlCtrl = CreateWindow(
        MC_WC_HTML,
        url.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0,
        hWndParent,
        (HMENU)IDC_HTML_CTRL,
        g_hInst,
        NULL);
}

HWND GetHTMLControl() {
    return g_hHtmlCtrl;
}

//==== Command line and deeplink helpers ==================================================

// These deeplinks should make the list in https://github.com/Psiphon-Inc/psiphon-issues/wiki/Supported-Psiphon-client-deep-links
static const wstring DEEPLINK_PROTOCOL_SCHEME = L"psiphon";
static const vector<wstring> ALLOWED_DEEPLINKS = {
    DEEPLINK_PROTOCOL_SCHEME + L"://psicash",
    DEEPLINK_PROTOCOL_SCHEME + L"://psicash/buy",
    DEEPLINK_PROTOCOL_SCHEME + L"://feedback",
    DEEPLINK_PROTOCOL_SCHEME + L"://subscribe",
    DEEPLINK_PROTOCOL_SCHEME + L"://settings",
    DEEPLINK_PROTOCOL_SCHEME + L"://settings/systray-minimize",
    DEEPLINK_PROTOCOL_SCHEME + L"://settings/disallowed-traffic-alert",
    DEEPLINK_PROTOCOL_SCHEME + L"://settings/split-tunnel",
    DEEPLINK_PROTOCOL_SCHEME + L"://settings/disable-timeouts",
    DEEPLINK_PROTOCOL_SCHEME + L"://settings/egress-region",
    DEEPLINK_PROTOCOL_SCHEME + L"://settings/local-proxy-ports",
    DEEPLINK_PROTOCOL_SCHEME + L"://settings/upstream-proxy",
    DEEPLINK_PROTOCOL_SCHEME + L"://settings/transport-mode"
};
inline size_t MaxDeeplinkLength() {
    auto longestDeeplink = std::max_element(ALLOWED_DEEPLINKS.begin(), ALLOWED_DEEPLINKS.end(), [](const auto& s1, const auto& s2) {
        return s1.length() < s2.length();
    });
    return longestDeeplink->length();
}
inline size_t MaxCommandLineLength() {
    // our command line is like `-- "<max_deeplink>"`
    return MaxDeeplinkLength() + L"-- \"\""s.length();
}

// Using a big messy value for this rather than something like `1` to help ensure we don't try to
// process a WM_COPYDATA payload not intended for us.
constexpr ULONG_PTR COPY_DATA_CMDLINE = 0x3AB21AEF;


void RegisterPsiphonProtocolHandler() {
    if (!WriteRegistryProtocolHandler(DEEPLINK_PROTOCOL_SCHEME)) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: WriteRegistryProtocolHandler failed to register deeplink protocol"), __TFUNCTION__, __LINE__);
        // This isn't fatal to the application functioning, so no error will be produced
    }
}

static void HTMLUI_Deeplink(const string& deeplinkJSON);

static void DoDeeplink(const wstring& untrustedDeeplink)
{
    // We're only going to send known-good deeplinks to the HTML UI, but we're going to be
    // a bit flexible about matching the incoming the links. This is for two reasons:
    // 1. Windows adds (sometimes? always?) a trailing slash to an authority-only URL,
    //    like `psiphon://psicash/`, but our cross-platform standard is to no have that
    //    trailing slash.
    // 2. If we get an overly-specific URL that we don't support, for example:
    //    - `psiphon://psicash/speedboost` -> `psiphon://psicash`, which is a link supported by iOS and Android
    //    - `psiphon://settings/nope` -> `psiphon://settings`, in case we get a settings deeplink for another platform
    //
    // So we're going to do prefix matching against the allowed deeplinks, sorted by
    // length descending, to match the more-specific ones first.
    vector<wstring> sortedDeeplinks = ALLOWED_DEEPLINKS;
    std::sort(
        sortedDeeplinks.begin(), sortedDeeplinks.end(),
        [](const wstring& first, const wstring& second){
            return first.length() > second.length(); // longest to shortest
        });

    wstring sanitizedDeeplink;
    for (const auto& deeplink : sortedDeeplinks) {
        // Check if this allowed deeplink is a prefix of (or equal to) the untrustedDeeplink
        if (untrustedDeeplink.rfind(deeplink, 0) == 0) {
            sanitizedDeeplink = deeplink;
            break;
        }

        // No match, so keep checking through the shorter links
    }

    if (sanitizedDeeplink.empty()) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: no match among sanitized deeplinks"), __TFUNCTION__, __LINE__);
        return;
    }

    if (!g_htmlUiReady) {
        // There is a race condition here: g_htmlUiReady could be set to true and (the
        // empty) g_queuedDeeplink could be processed before the following line completes.
        // We're not going to worry about it.
        g_queuedDeeplink = sanitizedDeeplink;
        my_print(NOT_SENSITIVE, true, _T("%s:%d: queued deeplink"), __TFUNCTION__, __LINE__);
        return;
    }

    my_print(NOT_SENSITIVE, true, _T("%s:%d: sending deeplink to UI"), __TFUNCTION__, __LINE__);

    Json::Value json;
    json["deeplink"] = WStringToUTF8(sanitizedDeeplink);
    Json::FastWriter jsonWriter;
    string strJSON = jsonWriter.write(json);

    HTMLUI_Deeplink(strJSON);
}


void ProcessCommandLine(const wstring& cmdLine) {
    // NOTE: Don't print the command line in any logs in this function,
    // just in case it's malicious or contains PII.

    if (cmdLine.length() > MaxCommandLineLength()) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: command line too long: %d"), __TFUNCTION__, __LINE__, cmdLine.length());
        return;
    }

    // We only have one kind of command line arg: deeplinks of the form `-- "psiphon://a/b"`.
    const wstring deeplinkPrefix = L"-- \"";
    const wstring deeplinkSuffix = L"\"";

    if (cmdLine.length() < (deeplinkPrefix.length() + deeplinkSuffix.length())) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: command line too short: %d"), __TFUNCTION__, __LINE__, cmdLine.length());
        return;
    }

    if (cmdLine.find(deeplinkPrefix) != 0) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: command line does not start with deeplink prefix"), __TFUNCTION__, __LINE__);
        return;
    }

    if (cmdLine.back() != L'"') {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: command line does not end with quote"), __TFUNCTION__, __LINE__);
        return;
    }

    // Strip the prefix and quotes
    auto deeplink = cmdLine.substr(
        deeplinkPrefix.length(),
        cmdLine.length() - (deeplinkPrefix.length() + deeplinkSuffix.length()));

    DoDeeplink(deeplink);
}

void ProcessCommandLineMessage(WPARAM wParam, LPARAM lParam) {
    if (!lParam) {
        return;
    }

    PCOPYDATASTRUCT pCopyDataStruct = (PCOPYDATASTRUCT)lParam;

    if (pCopyDataStruct->dwData != COPY_DATA_CMDLINE) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: received non-command line data: %d"), __TFUNCTION__, __LINE__, pCopyDataStruct->dwData);
        return;
    }

    // All +1s below are for the null terminator

    if (pCopyDataStruct->cbData > (sizeof(wchar_t) * (MaxCommandLineLength()+1))) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: received command line too long: %d"), __TFUNCTION__, __LINE__, pCopyDataStruct->cbData);
        return;
    }

    size_t cmdLineLen = wcsnlen_s((wchar_t*)pCopyDataStruct->lpData, pCopyDataStruct->cbData);
    if (pCopyDataStruct->cbData != (sizeof(wchar_t) * (cmdLineLen+1))) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: received command line buffer size is not equal to string length: %d != %d"), __TFUNCTION__, __LINE__, pCopyDataStruct->cbData, cmdLineLen+1);
        return;
    }

    // Our process doesn't own pCopyDataStruct->lpData, so this is the point that we make
    // a copy of it.
    wstring cmdLine((wchar_t*)pCopyDataStruct->lpData, cmdLineLen);

    ProcessCommandLine(cmdLine);
}

void SendCommandLineToWnd(HWND hWnd, wchar_t* lpCmdLine) {
    // Note that lpCmdLine is untrusted.

    if (!lpCmdLine || !hWnd) {
        return;
    }

    size_t cmdLineLen = _tcslen(lpCmdLine);
    if (cmdLineLen == 0) {
        return;
    }
    else if (cmdLineLen > MaxCommandLineLength()) {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: command line too long: %d"), __TFUNCTION__, __LINE__, cmdLineLen);
        return;
    }

    // WM_COPYDATA seems to be the (easiest) correct way to send data between processes
    COPYDATASTRUCT copyData = { 0 };
    copyData.dwData = COPY_DATA_CMDLINE;
    copyData.cbData = sizeof(wchar_t) * (cmdLineLen + 1); // include null terminator in length
    copyData.lpData = lpCmdLine;

    (void)SendMessage(hWnd, WM_COPYDATA, (WPARAM)g_hWnd, (LPARAM)(LPVOID)&copyData);
}

//==== String Table helpers ==================================================

static map<string, wstring> g_stringTable;

static void AddStringTableEntry(const string& utf8EntryJson)
{
    Json::Value json;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(utf8EntryJson, json);
    if (!parsingSuccessful)
    {
        my_print(NOT_SENSITIVE, true, _T("%s:%d: Failed to parse string table entry"), __TFUNCTION__, __LINE__);
        return;
    }

    string locale, key, narrowStr;

    try
    {
        if (!json.isMember("key") ||
            !json.isMember("string"))
        {
            // The stored values are invalid
            return;
        }

        locale = json.get("locale", "").asString();

        key = json.get("key", "").asString();
        narrowStr = json.get("string", "").asString();
        if (key.empty() || narrowStr.empty())
        {
            return;
        }
    }
    catch (exception& e)
    {
        my_print(NOT_SENSITIVE, false, _T("%s:%d: JSON parse exception: %S"), __TFUNCTION__, __LINE__, e.what());
        return;
    }

    auto wideStr = UTF8ToWString(narrowStr);
    g_stringTable[key] = wideStr;

    if (locale != g_uiLocale) {
        g_uiLocale = locale;

        SetUiLocale(UTF8ToWString(locale)); // used in feedback

        if (g_psiCashInitializd) {
            if (auto err = psicash::Lib::_().SetLocale(locale)) {
                // Log and carry on
                my_print(NOT_SENSITIVE, false, _T("%s: PsiCashLib::SetLocale failed, %hs"), __TFUNCTION__, err.ToString().c_str());
            }
        }
    }

    // As soon as the OS_UNSUPPORTED string is available, do the OS check.
    if (key == STRING_KEY_OS_UNSUPPORTED) {
        EnforceOSSupport(g_hWnd, wideStr);
    }
}

// Returns true if the string table entry is found, false otherwise.
bool GetStringTableEntry(const string& key, wstring& o_entry)
{
    o_entry.clear();

    map<string, wstring>::const_iterator iter = g_stringTable.find(key);
    if (iter == g_stringTable.end())
    {
        return false;
    }

    o_entry = iter->second;

    return true;
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

void HtmlUI_AddLog(int priority, LPCTSTR message)
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
    PostMessage(g_hWnd, WM_PSIPHON_HTMLUI_ADDLOG, (WPARAM)buf, 0);
}

static void HtmlUI_AddLogHandler(LPCWSTR json)
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
    if (!SendMessage(
        g_hHtmlCtrl, MC_HM_CALLSCRIPTFUNC,
        (WPARAM)_T("HtmlCtrlInterface_AddLog"), (LPARAM)&argStruct))
    {
        throw std::exception("UI: HtmlCtrlInterface_AddLog not found");
    }
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
    if (!SendMessage(
        g_hHtmlCtrl, MC_HM_CALLSCRIPTFUNC,
        (WPARAM)_T("HtmlCtrlInterface_SetState"), (LPARAM)&argStruct))
    {
        throw std::exception("UI: HtmlCtrlInterface_SetState not found");
    }
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
    if (!SendMessage(
        g_hHtmlCtrl, MC_HM_CALLSCRIPTFUNC,
        (WPARAM)_T("HtmlCtrlInterface_AddNotice"), (LPARAM)&argStruct))
    {
        throw std::exception("UI: HtmlCtrlInterface_AddNotice not found");
    }
    delete[] json;
}

static void HtmlUI_RefreshSettings(const string& settingsJSON)
{
    wstring wJson = UTF8ToWString(settingsJSON.c_str());

    size_t bufLen = wJson.length() + 1;
    wchar_t* buf = new wchar_t[bufLen];
    wcsncpy_s(buf, bufLen, wJson.c_str(), bufLen);
    buf[bufLen - 1] = L'\0';
    PostMessage(g_hWnd, WM_PSIPHON_HTMLUI_REFRESHSETTINGS, (WPARAM)buf, 0);
}

static void HtmlUI_RefreshSettingsHandler(LPCWSTR json)
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
    if (!SendMessage(
        g_hHtmlCtrl, MC_HM_CALLSCRIPTFUNC,
        (WPARAM)_T("HtmlCtrlInterface_RefreshSettings"), (LPARAM)&argStruct))
    {
        throw std::exception("UI: HtmlCtrlInterface_RefreshSettings not found");
    }
    delete[] json;
}

static void HtmlUI_UpdateDpiScaling(const string& dpiScalingJSON)
{
    wstring wJson = UTF8ToWString(dpiScalingJSON.c_str());

    size_t bufLen = wJson.length() + 1;
    wchar_t* buf = new wchar_t[bufLen];
    wcsncpy_s(buf, bufLen, wJson.c_str(), bufLen);
    buf[bufLen - 1] = L'\0';
    PostMessage(g_hWnd, WM_PSIPHON_HTMLUI_UPDATEDPISCALING, (WPARAM)buf, 0);
}

static void HtmlUI_UpdateDpiScalingHandler(LPCWSTR json)
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
    if (!SendMessage(
        g_hHtmlCtrl, MC_HM_CALLSCRIPTFUNC,
        (WPARAM)_T("HtmlCtrlInterface_UpdateDpiScaling"), (LPARAM)&argStruct))
    {
        throw std::exception("UI: HtmlUI_UpdateDpiScaling not found");
    }
    delete[] json;
}

static void HTMLUI_Deeplink(const string& deeplinkJSON) {
    wstring wJson = UTF8ToWString(deeplinkJSON.c_str());
    size_t bufLen = wJson.length() + 1;
    wchar_t* buf = new wchar_t[bufLen];
    wcsncpy_s(buf, bufLen, wJson.c_str(), bufLen);
    buf[bufLen - 1] = L'\0';
    PostMessage(g_hWnd, WM_PSIPHON_HTMLUI_DEEPLINK, (WPARAM)buf, 0);
}

static void HTMLUI_DeeplinkHandler(LPCWSTR deeplinkJSON) {
    if (!g_htmlUiReady)
    {
        delete[] deeplinkJSON;
        return;
    }

    MC_HMCALLSCRIPTFUNC argStruct = { 0 };
    argStruct.cbSize = sizeof(MC_HMCALLSCRIPTFUNC);
    argStruct.cArgs = 1;
    argStruct.pszArg1 = deeplinkJSON;
    if (!SendMessage(
        g_hHtmlCtrl, MC_HM_CALLSCRIPTFUNC,
        (WPARAM)_T("HtmlCtrlInterface_Deeplink"), (LPARAM)&argStruct))
    {
        throw std::exception("UI: HtmlUI_Deeplink not found");
    }
    delete[] deeplinkJSON;
}

static void HtmlUI_PsiCashMessage(const string& psicashJSON)
{
    wstring wJson = UTF8ToWString(psicashJSON.c_str());

    size_t bufLen = wJson.length() + 1;
    wchar_t* buf = new wchar_t[bufLen];
    wcsncpy_s(buf, bufLen, wJson.c_str(), bufLen);
    buf[bufLen - 1] = L'\0';
    PostMessage(g_hWnd, WM_PSIPHON_HTMLUI_PSICASHMESSAGE, (WPARAM)buf, 0);
}

static void HtmlUI_PsiCashMessageHandler(LPCWSTR json)
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
    if (!SendMessage(
        g_hHtmlCtrl, MC_HM_CALLSCRIPTFUNC,
        (WPARAM)_T("HtmlCtrlInterface_PsiCashMessage"), (LPARAM)&argStruct))
    {
        throw std::exception("UI: HtmlCtrlInterface_PsiCashMessage not found");
    }
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

// Helper for HtmlUI_BeforeNavigateHandler
static string uiURLParams(const wstring& url, size_t prefixLen) {
    if (url.length() <= prefixLen) {
        return "";
    }
    return Base64Decode(WStringToUTF8(url).substr(prefixLen));
}

// HtmlUI_BeforeNavigateHandler intercepts all navigation attempts in the HTML control.
// It is also this mechanism that is used for the HTML control to communicate
// with the back-end code (the code you're looking at now).
#define PSIPHON_LINK_PREFIX     _T("psi:")
static void HtmlUI_BeforeNavigateHandler(LPCTSTR _url)
{
    // NOTE: DO NOT log the URL directly -- it may contain the PsiCash password

    wstring url = UrlDecode(_url);
    delete[] _url;

    const LPCTSTR appReady = PSIPHON_LINK_PREFIX _T("ready");
    const LPCTSTR appStringTable = PSIPHON_LINK_PREFIX _T("stringtable?");
    const size_t appStringTableLen = _tcslen(appStringTable);
    const LPCTSTR appLogCommand = PSIPHON_LINK_PREFIX _T("log?");
    const size_t appLogCommandLen = _tcslen(appLogCommand);
    const LPCTSTR appStart = PSIPHON_LINK_PREFIX _T("start");
    const LPCTSTR appStop = PSIPHON_LINK_PREFIX _T("stop");
    const LPCTSTR appReconnect = PSIPHON_LINK_PREFIX _T("reconnect?");
    const size_t appReconnectLen = _tcslen(appReconnect);
    const LPCTSTR appSaveSettings = PSIPHON_LINK_PREFIX _T("savesettings?");
    const size_t appSaveSettingsLen = _tcslen(appSaveSettings);
    const LPCTSTR appSendFeedback = PSIPHON_LINK_PREFIX _T("sendfeedback?");
    const size_t appSendFeedbackLen = _tcslen(appSendFeedback);
    const LPCTSTR appSetCookies = PSIPHON_LINK_PREFIX _T("setcookies?");
    const size_t appSetCookiesLen = _tcslen(appSetCookies);
    const LPCTSTR appBannerClick = PSIPHON_LINK_PREFIX _T("bannerclick");
    const LPCTSTR psicashCommand = PSIPHON_LINK_PREFIX _T("psicash?");
    const size_t psicashCommandLen = _tcslen(psicashCommand);
    const LPCTSTR disallowedTraffic = PSIPHON_LINK_PREFIX _T("disallowedtraffic");

    if (url == appReady)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Ready requested"), __TFUNCTION__);
        g_htmlUiReady = true;
        InitPsiCash();
        PostMessage(g_hWnd, WM_PSIPHON_CREATED, 0, 0);

        if (!g_queuedDeeplink.empty()) {
            my_print(NOT_SENSITIVE, true, _T("%s: Opening queued deeplink"), __TFUNCTION__);
            DoDeeplink(g_queuedDeeplink);
            g_queuedDeeplink.clear();
        }
    }
    else if (url.find(appStringTable) == 0 && url.length() > appStringTableLen)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: String table addition requested"), __TFUNCTION__);

        string stringJSON = uiURLParams(url, appStringTableLen);
        AddStringTableEntry(stringJSON);
    }
    else if (url.find(appLogCommand) == 0 && url.length() > appLogCommandLen)
    {
        string log = uiURLParams(url, appLogCommandLen);
        my_print(NOT_SENSITIVE, true, _T("UILog: %S"), log.c_str());
    }
    else if (url == appStart)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Start requested"), __TFUNCTION__);
        g_connectionManager.Start();
    }
    else if (url == appStop)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Stop requested"), __TFUNCTION__);
        g_connectionManager.Stop(STOP_REASON_USER_DISCONNECT);
    }
    else if (url.find(appReconnect) == 0 && url.length() > appReconnectLen)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Reconnect requested"), __TFUNCTION__);

        string params = uiURLParams(url, appReconnectLen);
        g_connectionManager.Reconnect(params == "suppress=1");
    }
    else if (url.find(appSaveSettings) == 0 && url.length() > appSaveSettingsLen)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Save settings requested"), __TFUNCTION__);

        string stringJSON = uiURLParams(url, appSaveSettingsLen);
        bool reconnectRequired = false;
        bool success = Settings::FromJson(stringJSON, reconnectRequired);

        bool doReconnect = success && reconnectRequired &&
            (g_connectionManager.GetState() == CONNECTION_MANAGER_STATE_CONNECTED
                || g_connectionManager.GetState() == CONNECTION_MANAGER_STATE_STARTING);

        // refresh the settings in the UI
        Json::Value settingsJSON;
        Settings::ToJson(settingsJSON);

        Json::Value settingsRefreshJSON;
        settingsRefreshJSON["settings"] = settingsJSON;
        settingsRefreshJSON["success"] = success;
        settingsRefreshJSON["reconnectRequired"] = doReconnect;

        Json::FastWriter jsonWriter;
        string strSettingsRefreshJSON = jsonWriter.write(settingsRefreshJSON);
        UI_RefreshSettings(strSettingsRefreshJSON);

        if (doReconnect)
        {
            // Instead of reconnecting here, we could let the JS see that it's
            // required and then trigger it. But that seems like an unnecessary round-trip.
            my_print(NOT_SENSITIVE, false, _T("Settings change detected. Reconnecting."));
            g_connectionManager.Reconnect(false);
        }
    }
    else if (url.find(appSendFeedback) == 0 && url.length() > appSendFeedbackLen)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Send feedback requested"), __TFUNCTION__);
        my_print(NOT_SENSITIVE, false, _T("Sending feedback..."));

        string feedbackJSON = uiURLParams(url, appSendFeedbackLen);
        g_connectionManager.SendFeedback(feedbackJSON.c_str());
    }
    else if (url.find(appSetCookies) == 0 && url.length() > appSetCookiesLen)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Set cookies requested"), __TFUNCTION__);

        string stringJSON = uiURLParams(url, appSetCookiesLen);
        Settings::SetCookies(stringJSON);
    }
    else if (url == appBannerClick)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: Banner clicked"), __TFUNCTION__);
        // If connected, open sponsor home pages, or info link if
        // no sponsor pages. If not connected, open info link.
        if (CONNECTION_MANAGER_STATE_CONNECTED == g_connectionManager.GetState())
        {
            g_connectionManager.OpenHomePages("banner", INFO_LINK_URL);
        }
        else
        {
            OpenBrowser(INFO_LINK_URL);
        }
    }
    else if (url.find(psicashCommand) == 0 && url.length() > psicashCommandLen)
    {
        my_print(NOT_SENSITIVE, true, _T("%s: PsiCash command requested"), __TFUNCTION__);

        string stringJSON = uiURLParams(url, psicashCommandLen);
        if (!HandlePsiCashCommand(stringJSON))
        {
            my_print(NOT_SENSITIVE, true, _T("%s: HandlePsiCashCommand failed"), __TFUNCTION__);
        }
    }
    else if (url == disallowedTraffic)
    {
        // We got a disallowed-traffic alert. Foreground and show systray notification.

        ForegroundWindow();

        wstring infoTitle, infoBody;
        (void)GetStringTableEntry(STRING_KEY_DISALLOWED_TRAFFIC_NOTIFICATION_TITLE, infoTitle);
        (void)GetStringTableEntry(STRING_KEY_DISALLOWED_TRAFFIC_NOTIFICATION_BODY, infoBody);

        UpdateSystrayIcon(NULL, infoTitle, infoBody);
    }
    else {
        // Not one of our links. Open it in an external browser.
        OpenBrowser(url);

        // Copy the URL to the clipboard and let the UI know we did so, so the user can be informed
        if (CopyToClipboard(g_hWnd, url)) {
            UI_Notice("PsiphonUI::URLCopiedToClipboard", WStringToUTF8(url));
        }

        my_print(NOT_SENSITIVE, true, _T("%s: external URL opened and copied to clipboard"), __TFUNCTION__);
    }
}

//==== Exported functions ========================================================

void UI_SetStateStopped()
{
    UpdateSystrayConnectedState();
    ResetConnectedReminderTimer();

    Json::Value json;
    json["state"] = "stopped";
    Json::FastWriter jsonWriter;
    wstring wJson = UTF8ToWString(jsonWriter.write(json).c_str());
    HtmlUI_SetState(wJson);
}

void UI_SetStateStopping()
{
    UpdateSystrayConnectedState();
    ResetConnectedReminderTimer();

    Json::Value json;
    json["state"] = "stopping";
    Json::FastWriter jsonWriter;
    wstring wJson = UTF8ToWString(jsonWriter.write(json).c_str());
    HtmlUI_SetState(wJson);
}

void UI_SetStateStarting(const tstring& transportProtocolName)
{
    UpdateSystrayConnectedState();

    Json::Value json;
    json["state"] = "starting";
    json["transport"] = WStringToUTF8(transportProtocolName.c_str());
    Json::FastWriter jsonWriter;
    wstring wJson = UTF8ToWString(jsonWriter.write(json).c_str());
    HtmlUI_SetState(wJson);
}

void UI_SetStateConnected(const tstring& transportProtocolName, int socksPort, int httpPort)
{
    UpdateSystrayConnectedState();
    StartConnectedReminderTimer();

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

// Take JSON in the form provided by CoreTransport
void UI_Notice(const string& noticeJSON)
{
    HtmlUI_AddNotice(noticeJSON);
}

// This is a helper to construct the JSON required for the previous function.
// Notice ID must be unique. (Recommended: Prefix it.)
// `techInfo` may be an empty string.
void UI_Notice(const string& noticeID, const string& techInfo)
{
    Json::Value json;
    json["noticeType"] = noticeID;
    json["data"] = techInfo;
    Json::FastWriter jsonWriter;
    UI_Notice(jsonWriter.write(json));
}

void UI_RefreshSettings(const string& settingsJSON)
{
    HtmlUI_RefreshSettings(settingsJSON);
}

void UI_UpdateDpiScaling(const string& dpiScalingJSON)
{
    HtmlUI_UpdateDpiScaling(dpiScalingJSON);
}

//
// PsiCash
//

enum class PsiCashMessageType {
    REFRESH,
    NEW_PURCHASE,
    INIT_DONE,
    ACCOUNT_LOGIN,
    ACCOUNT_LOGOUT
};

static map<PsiCashMessageType, string> PsiCashMessageTypeNames = {
    {PsiCashMessageType::REFRESH, "refresh"},
    {PsiCashMessageType::NEW_PURCHASE, "new-purchase"},
    {PsiCashMessageType::INIT_DONE, "init-done"},
    {PsiCashMessageType::ACCOUNT_LOGIN, "account-login"},
    {PsiCashMessageType::ACCOUNT_LOGOUT, "account-logout"},
};

struct PsiCashMessage {
    PsiCashMessageType type;
    string id; // unused if empty
    nlohmann::json payload;

    PsiCashMessage(PsiCashMessageType type, const string& id) : type(type), id(id), payload({}) {}

    bool JSON(string& s) const {
        try {
            auto json = nlohmann::json{
                { "type",    PsiCashMessageTypeNames[this->type] },
                { "id",      this->id },
                { "payload", this->payload } };
            s = json.dump(-1, ' ', true);
        }
        catch (nlohmann::json::exception& e) {
            my_print(NOT_SENSITIVE, true, _T("%s: json dump failed: %hs; id: %d"), __TFUNCTION__, e.what(), e.id);
            return false;
        }
        return true;
    }
};

// Initialize the PsiCash library. Must be called after the UI is ready.
void InitPsiCash() {
    PsiCashMessage evt(PsiCashMessageType::INIT_DONE, "");

    g_psiCashInitializd = false;
    if (auto err = psicash::Lib::_().Init(false)) {
        // Init failed, indicating file corruption or disk access problems.
        // We'll try to reset.
        auto retryErr = psicash::Lib::_().Init(true);
        g_psiCashInitializd = !retryErr;

        // At this point Init may have failed again, and we'll be proceeding without any PsiCash support

        my_print(NOT_SENSITIVE, false, _T("%s: PsiCashLib initialization failed, %s: %hs"), __TFUNCTION__, (retryErr ? _T("unrecovered") : _T("recovered")), err.ToString().c_str());

        // Tell the UI about the init failure, so it can display a message
        nlohmann::json jsonResult;
        jsonResult["error"] = err.ToString(); // display the origin error, even if the retry succeeded
        jsonResult["recovered"] = retryErr ? false : true;
        evt.payload = jsonResult;
    }
    else
    {
        g_psiCashInitializd = true;
        my_print(NOT_SENSITIVE, true, _T("%s: PsiCashLib initialization succeeded"), __TFUNCTION__);
    }

    if (g_psiCashInitializd) {
        if (auto err = psicash::Lib::_().SetLocale(g_uiLocale)) {
            // Log and carry on
            my_print(NOT_SENSITIVE, false, _T("%s: PsiCashLib::SetLocale failed, %hs"), __TFUNCTION__, err.ToString().c_str());
        }
    }

    string jsonString;
    if (!evt.JSON(jsonString)) {
        my_print(NOT_SENSITIVE, true, _T("%s: PsiCashMessage.JSON failed"), __TFUNCTION__);
        return;
    }

    HtmlUI_PsiCashMessage(jsonString);
}

nlohmann::json MakeRefreshPsiCashPayload() {
    nlohmann::json res = {
        { "is_account", psicash::Lib::_().IsAccount() },
        { "has_tokens", psicash::Lib::_().HasTokens() },
        { "has_tokens", psicash::Lib::_().HasTokens() },
        { "balance",  psicash::Lib::_().Balance() },
        { "purchase_prices", psicash::Lib::_().GetPurchasePrices() },
        { "purchases", psicash::Lib::_().GetPurchases() },
        { "account_signup_url", psicash::Lib::_().GetUserSiteURL(psicash::PsiCash::UserSiteURLType::AccountSignup, false) },
        { "account_management_url", psicash::Lib::_().GetUserSiteURL(psicash::PsiCash::UserSiteURLType::AccountManagement, false) },
        { "forgot_account_url", psicash::Lib::_().GetUserSiteURL(psicash::PsiCash::UserSiteURLType::ForgotAccount, false) },

        // Trying to use ternary conditionals for these will cause a crash
        { "account_username", nullptr },
        { "buy_psi_url", nullptr }
    };

    nonstd::optional<string> accountUsernameOpt = psicash::Lib::_().AccountUsername();
    if (accountUsernameOpt) {
        res["account_username"] = *accountUsernameOpt;
    }

    auto buyPsiURLOpt = psicash::Lib::_().GetBuyPsiURL();
    if (buyPsiURLOpt) {
        res["buy_psi_url"] = *buyPsiURLOpt;
    }

    return res;
}

// Exported function.
// commandID may be empty if not needed.
void UI_RefreshPsiCash(const string& commandID, bool reconnect_required)
{
    PsiCashMessage evt(PsiCashMessageType::REFRESH, commandID);
    evt.payload = MakeRefreshPsiCashPayload();

    evt.payload["reconnect_required"] = reconnect_required;

    string jsonString;
    if (!evt.JSON(jsonString)) {
        my_print(NOT_SENSITIVE, true, _T("%s: PsiCashMessage.JSON failed"), __TFUNCTION__);
        return;
    }

    HtmlUI_PsiCashMessage(jsonString);
}

bool HandlePsiCashCommand(const string& jsonString)
{
    Json::Value json;
    Json::Reader reader;

    bool parsingSuccessful = reader.parse(jsonString, json);
    if (!parsingSuccessful)
    {
        my_print(NOT_SENSITIVE, false, _T("%s: Failed to parse PsiCash command"), __TFUNCTION__);
        return false;
    }

    string commandID;
    if (json["id"].isString())
    {
        commandID = json["id"].asString();
    }

    if (json["command"] == "refresh")
    {
        bool localOnly = g_connectionManager.GetState() != CONNECTION_MANAGER_STATE_CONNECTED;
        my_print(NOT_SENSITIVE, true, _T("%s: PsiCash::RefreshState; local-only: %S; reason: %S"), __TFUNCTION__, (localOnly ? "yes" : "no"), json["reason"].asCString());

        // We're connected, so ask the server for fresh info
        psicash::Lib::_().RefreshState(localOnly, [commandID](psicash::error::Result<psicash::PsiCash::RefreshStateResponse> result)
        {
            // NOTE: This callback is (likely) _not_ on the same thread as the original call.

            bool reconnect_required = false;
            if (result) {
                reconnect_required = result->reconnect_required;
            }
            else {
                my_print(NOT_SENSITIVE, true, _T("%s: PsiCash::RefreshState failed: %S"), __TFUNCTION__, result.error().ToString().c_str());
            }

            // Refreshing the UI regardless of request result, as there might still
            // good data cached locally.
            UI_RefreshPsiCash(commandID, reconnect_required);
        });
    }
    else if (json["command"] == "purchase")
    {
        psicash::Lib::_().NewExpiringPurchase(
            json["transactionClass"].asString(), json["distinguisher"].asString(), json["expectedPrice"].asInt64(),
            [commandID](psicash::error::Result<psicash::PsiCash::NewExpiringPurchaseResponse> result)
        {
            // NOTE: This callback is (likely) _not_ on the same thread as the original call.

            // Send the result through to the JS to sort out.

            PsiCashMessage evt(PsiCashMessageType::NEW_PURCHASE, commandID);

            nlohmann::json jsonResult;
            if (!result) {
                jsonResult["error"] = result.error().ToString();
                jsonResult["status"] = -1;
            }
            else {
                jsonResult["error"] = nullptr;
                jsonResult["status"] = result->status;
                jsonResult["refresh"] = MakeRefreshPsiCashPayload();

                // If the purchase is successful and there's an authorization to apply to the tunnel, we'll tell the UI to trigger a reconnect
                if (result->status == psicash::Status::Success &&
                    result->purchase && result->purchase->authorization) {
                    jsonResult["refresh"]["reconnect_required"] = true;
                }
            }

            evt.payload = jsonResult;

            string jsonString;
            if (!evt.JSON(jsonString)) {
                my_print(NOT_SENSITIVE, true, _T("%s: PsiCashMessage.JSON failed"), __TFUNCTION__);
                return;
            }

            HtmlUI_PsiCashMessage(jsonString);
            my_print(NOT_SENSITIVE, true, _T("%s: NewExpiringPurchase result: %hs"), __TFUNCTION__, jsonString.c_str());
        });
    }
    else if (json["command"] == "login")
    {
        psicash::Lib::_().AccountLogin(
            json["username"].asString(), json["password"].asString(),
            [commandID](psicash::error::Result<psicash::PsiCash::AccountLoginResponse> result) {
                // NOTE: This callback is (likely) _not_ on the same thread as the original call.

                // Send the result through to the JS to sort out.

                PsiCashMessage evt(PsiCashMessageType::ACCOUNT_LOGIN, commandID);

                nlohmann::json jsonResult;
                if (!result)
                {
                    jsonResult["error"] = result.error().ToString();
                    jsonResult["status"] = -1;
                }
                else
                {
                    jsonResult["error"] = nullptr;
                    jsonResult["status"] = result->status;
                    jsonResult["last_tracker_merge"] = result->last_tracker_merge ? *result->last_tracker_merge : nullptr;
                    jsonResult["refresh"] = MakeRefreshPsiCashPayload();
                }

                evt.payload = jsonResult;

                string jsonString;
                if (!evt.JSON(jsonString))
                {
                    my_print(NOT_SENSITIVE, true, _T("%s: PsiCashMessage.JSON failed"), __TFUNCTION__);
                    return;
                }

                HtmlUI_PsiCashMessage(jsonString);
                my_print(NOT_SENSITIVE, true, _T("%s: AccountLogin result: %hs"), __TFUNCTION__, jsonString.c_str());
            });
    }
    else if (json["command"] == "logout")
    {
        psicash::Lib::_().AccountLogout(
            [commandID](psicash::error::Result<psicash::PsiCash::AccountLogoutResponse> result) {
                // NOTE: This callback is (likely) _not_ on the same thread as the original call.

                // Send the result through to the JS to sort out.

                PsiCashMessage evt(PsiCashMessageType::ACCOUNT_LOGOUT, commandID);

                nlohmann::json jsonResult;
                if (!result)
                {
                    jsonResult["error"] = result.error().ToString();
                }
                else
                {
                    jsonResult["error"] = nullptr;
                    jsonResult["reconnect_required"] = result->reconnect_required;
                    jsonResult["refresh"] = MakeRefreshPsiCashPayload();

                }

                evt.payload = jsonResult;

                string jsonString;
                if (!evt.JSON(jsonString))
                {
                    my_print(NOT_SENSITIVE, true, _T("%s: PsiCashMessage.JSON failed"), __TFUNCTION__);
                    return;
                }

                HtmlUI_PsiCashMessage(jsonString);
                my_print(NOT_SENSITIVE, true, _T("%s: AccountLogout result: %hs"), __TFUNCTION__, jsonString.c_str());
            });
    }
    else
    {
        my_print(NOT_SENSITIVE, true, _T("%s: no command match: %hs"), __TFUNCTION__, jsonString.c_str());
    }

    return true;
}

// =======================================================
// Window message handlers

LRESULT HandleNotifyHTMLControl(HWND hWnd, NMHDR* hdr)
{
    if (!hdr || hdr->idFrom != IDC_HTML_CTRL)
    {
        return 0;
    }

    switch (hdr->code) {
    case MC_HN_BEFORENAVIGATE:
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

    case MC_HN_NEWWINDOW:
        //MC_NMHTMLURL* nmHtmlUrl = (MC_NMHTMLURL*)hdr;
        // Prevent new window from opening
        return 0;

    case MC_HN_HTTPERROR:
        //MC_NMHTTPERROR* nmHttpError = (MC_NMHTTPERROR*)hdr;
        assert(false);
        // Prevent HTTP error from being shown.
        return 0;
    }

    return 0;
}

void HTMLControlWndProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PSIPHON_HTMLUI_BEFORENAVIGATE:
        HtmlUI_BeforeNavigateHandler((LPCTSTR)wParam);
        break;
    case WM_PSIPHON_HTMLUI_SETSTATE:
        HtmlUI_SetStateHandler((LPCWSTR)wParam);
        break;
    case WM_PSIPHON_HTMLUI_ADDLOG:
        HtmlUI_AddLogHandler((LPCWSTR)wParam);
        break;
    case WM_PSIPHON_HTMLUI_ADDNOTICE:
        HtmlUI_AddNoticeHandler((LPCWSTR)wParam);
        break;
    case WM_PSIPHON_HTMLUI_REFRESHSETTINGS:
        HtmlUI_RefreshSettingsHandler((LPCWSTR)wParam);
        break;
    case WM_PSIPHON_HTMLUI_UPDATEDPISCALING:
        HtmlUI_UpdateDpiScalingHandler((LPCWSTR)wParam);
        break;
    case WM_PSIPHON_HTMLUI_DEEPLINK:
        HTMLUI_DeeplinkHandler((LPCWSTR)wParam);
        break;
    case WM_PSIPHON_HTMLUI_PSICASHMESSAGE:
        HtmlUI_PsiCashMessageHandler((LPCWSTR)wParam);
        break;
    };
}
