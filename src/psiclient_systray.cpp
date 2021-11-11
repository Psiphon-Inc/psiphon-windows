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
#include "psiclient_systray.h"
#include "psiclient.h"
#include "psiclient_ui.h"
#include "logging.h"
#include "usersettings.h"
#include "embeddedvalues.h"


//==== Systray/Notification helpers ===========================================

// General info on Notifications: https://msdn.microsoft.com/en-us/library/ee330740%28v=vs.85%29.aspx
// NOTIFYICONDATA: https://msdn.microsoft.com/en-us/library/bb773352%28v=vs.85%29.aspx
// Shell_NotifyIcon: https://msdn.microsoft.com/en-us/library/bb762159%28VS.85%29.aspx

// Not defined on older OSes
#ifndef NIIF_LARGE_ICON
#define NIIF_LARGE_ICON 0x00000020
#endif
#ifndef NIIF_USER
#define NIIF_USER       0x00000004
#endif

// Timer IDs
// Note: Trying to use SetTimer/KillTimer without an explicit ID led to inconsistent behaviour.
#define TIMER_ID_SYSTRAY_MINIMIZE       100
#define TIMER_ID_SYSTRAY_STATE_UPDATE   101
#define TIMER_ID_CONNECTED_REMINDER     102

static NOTIFYICONDATA g_notifyIconData = { 0 };
static HICON g_notifyIconStopped = NULL;
static HICON g_notifyIconConnected = NULL;
static bool g_notifyIconAdded = false;


// InitSystrayIcon initializes the systray icon/notification. Gets called by
// UpdateSystrayState and should not be called directly.
static void InitSystrayIcon()
{
    static bool s_initialized = false;
    if (s_initialized)
    {
        return;
    }
    s_initialized = true;

    g_notifyIconStopped = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_SYSTRAY_STOPPED));
    g_notifyIconConnected = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_SYSTRAY_CONNECTED));

    g_notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
    g_notifyIconData.hWnd = g_hWnd;
    g_notifyIconData.uID = 1;  // Used to identify multiple icons. We only use one.
    g_notifyIconData.uCallbackMessage = WM_PSIPHON_TRAY_ICON_NOTIFY;
    g_notifyIconData.uTimeout = 30000;  // 30s is the max time the balloon can show

    g_notifyIconData.hIcon = g_notifyIconStopped;  // Begin with the stopped icon.

    g_notifyIconData.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;

    _tcsncpy_s(
        g_notifyIconData.szTip,
        sizeof(g_notifyIconData.szTip) / sizeof(TCHAR),
        g_appTitle.c_str(),
        _TRUNCATE);

    g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

    g_notifyIconData.uVersion = NOTIFYICON_VERSION;
}

// UpdateSystrayIcon sets the current systray icon.
// If infoTitle is non-empty, then it will also display a balloon.
// If hIcon is NULL, then the icon will not be changed.
void UpdateSystrayIcon(HICON hIcon, const wstring& infoTitle, const wstring& infoBody,
                       boolean noSound/*=false*/, boolean connectedReminder/*=false*/)
{
    if (IsUIShutDown())
    {
        return;
    }

    InitSystrayIcon();

    // InitSystrayIcon only gets called once. Override defaults:

    g_notifyIconData.uCallbackMessage = connectedReminder ?
        WM_PSIPHON_TRAY_CONNECTED_REMINDER_NOTIFY : WM_PSIPHON_TRAY_ICON_NOTIFY;

    if (noSound)
    {
        g_notifyIconData.dwInfoFlags |= NIIF_NOSOUND;
    }
    else
    {
        g_notifyIconData.dwInfoFlags &= ~NIIF_NOSOUND;
    }

    // Prevent duplicate updates
    static HICON s_lastIcon = NULL;
    static wstring s_lastInfoTitle;
    static wstring s_lastInfoBody;

    if (hIcon == s_lastIcon && infoTitle == s_lastInfoTitle && infoBody == s_lastInfoBody)
    {
        if (!connectedReminder)
        {
            return;
        }
    }
    s_lastIcon = hIcon;
    s_lastInfoTitle = infoTitle;
    s_lastInfoBody = infoBody;

    // The body isn't allowed to be an empty string, so set it to a space.
    wstring infoBodyToUse = infoBody.empty() ? L" " : infoBody;

    if (!infoTitle.empty())
    {
        _tcsncpy_s(
            g_notifyIconData.szInfoTitle,
            sizeof(g_notifyIconData.szInfoTitle) / sizeof(TCHAR),
            infoTitle.c_str(),
            _TRUNCATE);

        _tcsncpy_s(
            g_notifyIconData.szInfo,
            sizeof(g_notifyIconData.szInfo) / sizeof(TCHAR),
            infoBodyToUse.c_str(),
            _TRUNCATE);

        g_notifyIconData.uFlags |= NIF_INFO;
    }
    else
    {
        // We don't have the info text (yet)
        g_notifyIconData.uFlags &= ~NIF_INFO;
    }

    if (hIcon != NULL)
    {
        g_notifyIconData.hIcon = hIcon;
    }

    // The way an existing systray icon ought to be updated is like:
    //   Shell_NotifyIcon(NIM_MODIFY, &g_notifyIconData);
    // But on Windows 10 this doesn't immediately update the icon or info tip
    // and instead waits for the previous one to expire, which can result in
    // a noticeable update delay (UI says connected, info tip still says connecting).
    // So instead we'll delete-and-recreate instead of updating.
    // On older versions of Windows, this has the unfortunate effect of causing
    // the systray icon to flash.

    if (g_notifyIconAdded)
    {
        (void)Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData);
    }

    BOOL bSuccess = Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);
    if (bSuccess)
    {
        (void)Shell_NotifyIcon(NIM_SETVERSION, &g_notifyIconData);
    }

    g_notifyIconAdded = true;
}

// SystrayIconCleanup must be called when the application is exiting.
void SystrayIconCleanup()
{
    if (!g_notifyIconAdded)
    {
        return;
    }

    (void)Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData);
    g_notifyIconAdded = false;
}

/*
In order to get the minimize animation, we will delay hiding the window until
after the minimize animation is complete.
*/

static UINT_PTR g_handleMinimizeTimerID = 0;

static VOID CALLBACK HandleMinimizeHelper(HWND hWnd, UINT, UINT_PTR idEvent, DWORD)
{
    assert(idEvent == g_handleMinimizeTimerID);

    ::KillTimer(hWnd, idEvent);
    g_handleMinimizeTimerID = 0;

    if (g_notifyIconAdded)
    {
        ShowWindow(g_hWnd, SW_HIDE);

        // Show a balloon letting the user know where the app went
        wstring infoTitle, infoBody;
        (void)GetStringTableEntry(STRING_KEY_MINIMIZED_TO_SYSTRAY_TITLE, infoTitle);
        (void)GetStringTableEntry(STRING_KEY_MINIMIZED_TO_SYSTRAY_BODY, infoBody);

        UpdateSystrayIcon(NULL, infoTitle, infoBody);
        my_print(NOT_SENSITIVE, true, _T("%s: systray updated"), __TFUNCTION__);
    }
}

void HandleMinimize()
{
    if (!Settings::SystrayMinimize())
    {
        return;
    }

    // The time on this is a rough guess at how long the minimize animation will take.
    if (g_handleMinimizeTimerID == 0)
    {
        g_handleMinimizeTimerID = ::SetTimer(
            g_hWnd,
            TIMER_ID_SYSTRAY_MINIMIZE,
            300,
            HandleMinimizeHelper);
    }
}

/*
The systray state updating is a bit complicated. We want to avoid this:
  When Psiphon quickly disconnects and reconnects, we don't want to spam the
  systray balloon text.
What we want is:
  When a disconnect occurs, we wait a bit before changing the systray state.
  When that wait expires, if the state has gone back to connected, we don't
  show anything. If it has changed, we show the change.
  (Unless the application window is foreground, because otherwise the UI and systray
  state mismatch will be weird.)
*/

static ConnectionManagerState g_UpdateSystrayConnectedState_LastState = (ConnectionManagerState)0xFFFFFFFF;

static void UpdateSystrayConnectedStateHelper()
{
    if (IsUIShutDown())
    {
        return;
    }

    ConnectionManagerState currentState = g_connectionManager.GetState();

    if (currentState == g_UpdateSystrayConnectedState_LastState)
    {
        return;
    }
    g_UpdateSystrayConnectedState_LastState = currentState;

    wstring infoTitle, infoBody, state;
    bool infoTitleFound = false, infoBodyFound = false;
    HICON hIcon = NULL;

    if (currentState == CONNECTION_MANAGER_STATE_CONNECTED)
    {
        hIcon = g_notifyIconConnected;
        infoTitleFound = GetStringTableEntry(STRING_KEY_STATE_CONNECTED_TITLE, infoTitle);
        infoBodyFound = GetStringTableEntry(STRING_KEY_STATE_CONNECTED_BODY, infoBody);
        state = L"connected";
    }
    else if (currentState == CONNECTION_MANAGER_STATE_STARTING)
    {
        hIcon = g_notifyIconStopped;
        infoTitleFound = GetStringTableEntry(STRING_KEY_STATE_STARTING_TITLE, infoTitle);
        infoBodyFound = GetStringTableEntry(STRING_KEY_STATE_STARTING_BODY, infoBody);
        state = L"starting";
    }
    else if (currentState == CONNECTION_MANAGER_STATE_STOPPING)
    {
        hIcon = g_notifyIconStopped;
        infoTitleFound = GetStringTableEntry(STRING_KEY_STATE_STOPPING_TITLE, infoTitle);
        infoBodyFound = GetStringTableEntry(STRING_KEY_STATE_STOPPING_BODY, infoBody);
        state = L"stopping";
    }
    else  // CONNECTION_MANAGER_STATE_STOPPED
    {
        hIcon = g_notifyIconStopped;
        infoTitleFound = GetStringTableEntry(STRING_KEY_STATE_STOPPED_TITLE, infoTitle);
        infoBodyFound = GetStringTableEntry(STRING_KEY_STATE_STOPPED_BODY, infoBody);
        state = L"stopped";
    }

    UpdateSystrayIcon(hIcon, infoTitle, infoBody);
    my_print(NOT_SENSITIVE, true, _T("%s: systray updated: %s"), __TFUNCTION__, state.c_str());

    // Set app icon to match
    PostMessage(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    PostMessage(g_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
}

static UINT_PTR g_updateSystrayConnectedStateTimerID = 0;

static VOID CALLBACK UpdateSystrayConnectedStateTimer(HWND hWnd, UINT, UINT_PTR idEvent, DWORD)
{
    assert(g_updateSystrayConnectedStateTimerID == idEvent);
    ::KillTimer(hWnd, idEvent);
    g_updateSystrayConnectedStateTimerID = 0;

    UpdateSystrayConnectedStateHelper();
}

void UpdateSystrayConnectedState()
{
    if (g_UpdateSystrayConnectedState_LastState == CONNECTION_MANAGER_STATE_CONNECTED
        && ::GetForegroundWindow() != g_hWnd)
    {
        // Don't keep resetting the timer once we've set it.
        if (g_updateSystrayConnectedStateTimerID == 0)
        {
            g_updateSystrayConnectedStateTimerID = ::SetTimer(
                g_hWnd,
                TIMER_ID_SYSTRAY_STATE_UPDATE,
                5000,
                UpdateSystrayConnectedStateTimer);
        }
    }
    else
    {
        // Just update the state now.
        UpdateSystrayConnectedStateHelper();
    }
}

UINT CONNECTED_REMINDER_LONG_INTERVAL_MS = 6 * 60 * 60 * 1000;
UINT CONNECTED_REMINDER_SHORT_INTERVAL_MS = 60 * 1000;
static UINT g_connectedReminderIntervalMs = CONNECTED_REMINDER_LONG_INTERVAL_MS;
static UINT_PTR g_showConnectedReminderBalloonTimerID = 0;
static VOID CALLBACK ShowConnectedReminderBalloonTimer(HWND hWnd, UINT, UINT_PTR idEvent, DWORD);

void StartConnectedReminderTimer()
{
    if (g_showConnectedReminderBalloonTimerID == 0)
    {
        g_showConnectedReminderBalloonTimerID = ::SetTimer(
            g_hWnd,
            TIMER_ID_CONNECTED_REMINDER,
            g_connectedReminderIntervalMs,
            ShowConnectedReminderBalloonTimer);
    }
}

static void StopConnectedReminderTimer()
{
    ::KillTimer(g_hWnd, TIMER_ID_CONNECTED_REMINDER);
    g_showConnectedReminderBalloonTimerID = 0;
}

void ResetConnectedReminderTimer()
{
    StopConnectedReminderTimer();
    g_connectedReminderIntervalMs = CONNECTED_REMINDER_LONG_INTERVAL_MS;
}

void RestartConnectedReminderTimer()
{
    ResetConnectedReminderTimer();
    StartConnectedReminderTimer();
}

static void ShowConnectedReminderBalloon()
{
    if (IsUIShutDown())
    {
        return;
    }

    // We'll interpret the presence of an authorization as an indication that
    // we have an active Speed Boost. (At this time there are no other uses
    // for authorizations in Windows. In the future we may need to examine
    // active purchases.)
    bool boosting = (g_connectionManager.GetAuthorizations().size() > 0);

    if (g_connectionManager.GetState() == CONNECTION_MANAGER_STATE_CONNECTED && !boosting)
    {
        HICON hIcon = g_notifyIconConnected;
        wstring infoTitle, infoBody;
        GetStringTableEntry(STRING_KEY_STATE_CONNECTED_REMINDER_TITLE, infoTitle);
        GetStringTableEntry(STRING_KEY_STATE_CONNECTED_REMINDER_BODY, infoBody);
        UpdateSystrayIcon(hIcon, infoTitle, infoBody, true, true);
    }
}

static VOID CALLBACK ShowConnectedReminderBalloonTimer(HWND hWnd, UINT, UINT_PTR idEvent, DWORD)
{
    assert(TIMER_ID_CONNECTED_REMINDER == idEvent);
    StopConnectedReminderTimer();

    ShowConnectedReminderBalloon();

    // Show the balloon again after a short interval
    g_connectedReminderIntervalMs = CONNECTED_REMINDER_SHORT_INTERVAL_MS;
    StartConnectedReminderTimer();
}

void SystrayWndProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PSIPHON_TRAY_CONNECTED_REMINDER_NOTIFY:
        if (lParam == NIN_BALLOONUSERCLICK &&
            CONNECTION_MANAGER_STATE_CONNECTED == g_connectionManager.GetState())
        {
            g_connectionManager.OpenHomePages("notification", INFO_LINK_URL);
            RestartConnectedReminderTimer();
            break;
        }
        // fall through to WM_PSIPHON_TRAY_ICON_NOTIFY

    case WM_PSIPHON_TRAY_ICON_NOTIFY:
        // Restore/foreground the app on any kind of click
        if (lParam == WM_LBUTTONUP ||
            lParam == WM_LBUTTONDBLCLK ||
            lParam == WM_RBUTTONUP ||
            lParam == WM_RBUTTONDBLCLK ||
            lParam == NIN_BALLOONUSERCLICK)
        {
            ForegroundWindow(g_hWnd);
        }
        break;
    }
}
