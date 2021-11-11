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

#pragma once

// HTML control-related windows messages
#define WM_PSIPHON_HTMLUI_BEFORENAVIGATE    WM_USER + 200
#define WM_PSIPHON_HTMLUI_SETSTATE          WM_USER + 201
#define WM_PSIPHON_HTMLUI_ADDLOG            WM_USER + 202
#define WM_PSIPHON_HTMLUI_ADDNOTICE         WM_USER + 203
#define WM_PSIPHON_HTMLUI_REFRESHSETTINGS   WM_USER + 204
#define WM_PSIPHON_HTMLUI_UPDATEDPISCALING  WM_USER + 205
#define WM_PSIPHON_HTMLUI_DEEPLINK          WM_USER + 206
#define WM_PSIPHON_HTMLUI_PSICASHMESSAGE    WM_USER + 207

/// Should be called during app initialization
void InitHTMLLib();

/// Should be called during app cleanup
void CleanupHTMLLib();

/// Should be called from OnCreate to create the main HTML control
void CreateHTMLControl(HWND hWndParent, float dpiScaling);

/// Retrieve a handle to the main HTML control
HWND GetHTMLControl();

/// Registers the `psiphon://` protocol handlers with the OS (used for deeplinking)
void RegisterPsiphonProtocolHandler();

/// Process command line arguments. Must be called every time the app is started,
/// _except_ if this is a non-primary instance that is about to be shut down
/// (in which case SendCommandLineToWnd() should be called).
void ProcessCommandLine(const wstring& cmdLine);

/// Process an incoming WM_COPYDATA that might have a command line payload.
void ProcessCommandLineMessage(WPARAM wParam, LPARAM lParam);

/// Send a message to `hWnd` with the given command line. This is used for passing commands
/// from a second application instance that is about to close itself to the first instance
/// that should process the command line.
void SendCommandLineToWnd(HWND hWnd, wchar_t* lpCmdLine);

/// Should be called from HandleNotify when hdr->idFrom == IDC_HTML_CTRL to process
/// notifications belonging to the main HTML control.
LRESULT HandleNotifyHTMLControl(HWND hWnd, NMHDR* hdr);

/// Should be called to process the above window messages
void HTMLControlWndProc(UINT message, WPARAM wParam, LPARAM lParam);

void HtmlUI_AddLog(int priority, LPCTSTR message);
void UI_UpdateDpiScaling(const std::string& dpiScalingJSON);

// String-table entries accessible via GetStringTableEntry
#define STRING_KEY_STATE_STOPPED_TITLE                      "appbackend#state-stopped-title"
#define STRING_KEY_STATE_STOPPED_BODY                       "appbackend#state-stopped-body"
#define STRING_KEY_STATE_STARTING_TITLE                     "appbackend#state-starting-title"
#define STRING_KEY_STATE_STARTING_BODY                      "appbackend#state-starting-body"
#define STRING_KEY_STATE_CONNECTED_TITLE                    "appbackend#state-connected-title"
#define STRING_KEY_STATE_CONNECTED_BODY                     "appbackend#state-connected-body"
#define STRING_KEY_STATE_CONNECTED_REMINDER_TITLE           "appbackend#state-connected-reminder-title"
#define STRING_KEY_STATE_CONNECTED_REMINDER_BODY            "appbackend#state-connected-reminder-body"
#define STRING_KEY_STATE_CONNECTED_REMINDER_BODY_2          "appbackend#state-connected-reminder-body-2"
#define STRING_KEY_STATE_STOPPING_TITLE                     "appbackend#state-stopping-title"
#define STRING_KEY_STATE_STOPPING_BODY                      "appbackend#state-stopping-body"
#define STRING_KEY_MINIMIZED_TO_SYSTRAY_TITLE               "appbackend#minimized-to-systray-title"
#define STRING_KEY_MINIMIZED_TO_SYSTRAY_BODY                "appbackend#minimized-to-systray-body"
#define STRING_KEY_OS_UNSUPPORTED                           "appbackend#os-unsupported"
#define STRING_KEY_DISALLOWED_TRAFFIC_NOTIFICATION_TITLE    "appbackend#disallowed-traffic-notification-title"
#define STRING_KEY_DISALLOWED_TRAFFIC_NOTIFICATION_BODY     "appbackend#disallowed-traffic-notification-body"

bool GetStringTableEntry(const std::string& key, std::wstring& o_entry);
