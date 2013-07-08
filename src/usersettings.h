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

#pragma once

#include <string>

using namespace std;

void InitializeUserSettings(void);
int GetUserSettingDword(const string& settingName, int defaultValue = 0);
string GetUserSettingString(const string& settingName, string defaultValue = string(""));
bool UserSkipBrowser(void);
bool UserSkipProxySettings(void);
int UserLocalHTTPProxyPort(void);
bool UserSkipSSHParentProxySettings(void);
string UserSSHParentProxyHostname(void);
int UserSSHParentProxyPort(void);
string UserSSHParentProxyUsername(void);
string UserSSHParentProxyPassword(void);
string UserSSHParentProxyType(void);



