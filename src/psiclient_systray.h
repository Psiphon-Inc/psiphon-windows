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

// Systray- and notification-related window messages
#define WM_PSIPHON_TRAY_ICON_NOTIFY                 WM_USER + 300
#define WM_PSIPHON_TRAY_CONNECTED_REMINDER_NOTIFY   WM_USER + 301

/// Should be called when the above window messages are received
void SystrayWndProc(UINT message, WPARAM wParam, LPARAM lParam);

/// Should be called during app cleanup
void SystrayIconCleanup();

/// Should be called when the app is being minimized
void HandleMinimize();

void UpdateSystrayConnectedState();
void StartConnectedReminderTimer();
void ResetConnectedReminderTimer();
void UpdateSystrayIcon(HICON hIcon, const wstring& infoTitle, const wstring& infoBody,
                       boolean noSound=false, boolean connectedReminder=false);
