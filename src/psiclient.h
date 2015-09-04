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

#pragma once

#include "resource.h"
#include "connectionmanager.h"


//==== global state ===================================================

// TODO: wrap this in a singleton interface?
extern ConnectionManager g_connectionManager;


//==== global window message constants =================================

#define WM_PSIPHON_MY_PRINT            WM_USER + 100
#define WM_PSIPHON_FEEDBACK_SUCCESS    WM_USER + 101
#define WM_PSIPHON_FEEDBACK_FAILED     WM_USER + 102
#define WM_PSIPHON_CREATED             WM_USER + 103
#define WM_PSIPHON_TRAY_ICON_NOTIFY    WM_USER + 104


//==== UI Interaction ==================================================

// Indicate connection state
void UI_SetStateStopped();
void UI_SetStateStopping();
void UI_SetStateStarting(const tstring& transportProtocolName);
void UI_SetStateConnected(const tstring& transportProtocolName, int socksPort, int httpPort);

// Send informational notice to the UI
void UI_Notice(const string& noticeJSON);
void UI_Notice(const string& noticeID, const string& techInfo);

// Indicate that the settings have change and should be refreshed in the UI
void UI_RefreshSettings(const string& settingsJSON);
