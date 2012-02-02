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

#include "stdafx.h"
#include "usersettings.h"
#include "config.h"


void InitializeUserSettings(void)
{
    // Read - and consequently write out default values for - all settings
    UserSkipBrowser();
    UserSkipProxySettings();
}


bool GetUserSetting(const string& settingName)
{
    bool settingValue = false;

    HKEY key = 0;
    DWORD value;
    DWORD bufferLength = sizeof(value);
    DWORD type;

    if (ERROR_SUCCESS == RegOpenKeyExA(HKEY_CURRENT_USER, TStringToNarrow(LOCAL_SETTINGS_REGISTRY_KEY).c_str(), 0, KEY_READ, &key) &&
        ERROR_SUCCESS == RegQueryValueExA(key, settingName.c_str(), 0, &type, (LPBYTE)&value, &bufferLength) &&
        type == REG_DWORD &&
        value == 1)
    {
        settingValue = true;
    }

    RegCloseKey(key);

    // Write out the setting with a "false" settingValue so that it's there
    // for users to see and use, if they want to set it.
    if (!settingValue)
    {
        SetUserSetting(settingName, false);
    }

    return settingValue;
}


void SetUserSetting(const string& settingName, bool settingValue)
{
    HKEY key = 0;
    DWORD value = settingValue ? 1 : 0;
    DWORD bufferLength = sizeof(value);

    RegOpenKeyExA(HKEY_CURRENT_USER, TStringToNarrow(LOCAL_SETTINGS_REGISTRY_KEY).c_str(), 0, KEY_SET_VALUE, &key);
    RegSetValueExA(key, settingName.c_str(), 0, REG_DWORD, (LPBYTE)&value, bufferLength);
    RegCloseKey(key);
}


bool UserSkipBrowser(void)
{
    return GetUserSetting(LOCAL_SETTINGS_REGISTRY_VALUE_USER_SKIP_BROWSER);
}


bool UserSkipProxySettings(void)
{
    return GetUserSetting(LOCAL_SETTINGS_REGISTRY_VALUE_USER_SKIP_PROXY_SETTINGS);
}
