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
#include "utilities.h"


void InitializeUserSettings(void)
{
    // Read - and consequently write out default values for - all settings
    UserSkipBrowser();
    UserSkipProxySettings();
    UserLocalHTTPProxyPort();
}


int GetUserSetting(const string& settingName, int defaultValue /* = 0 */)
{
    DWORD value = 0;

    if (!ReadRegistryDwordValue(settingName, value))
    {
        // Write out the setting with a default value so that it's there
        // for users to see and use, if they want to set it.
        value = defaultValue;
        WriteRegistryDwordValue(settingName, value);
    }

    return value;
}


bool UserSkipBrowser(void)
{
    return 1 == GetUserSetting(LOCAL_SETTINGS_REGISTRY_VALUE_USER_SKIP_BROWSER);
}


bool UserSkipProxySettings(void)
{
    return 1 == GetUserSetting(LOCAL_SETTINGS_REGISTRY_VALUE_USER_SKIP_PROXY_SETTINGS);
}

int UserLocalHTTPProxyPort(void)
{
    return GetUserSetting(LOCAL_SETTINGS_REGISTRY_VALUE_USER_LOCAL_HTTP_PROXY_PORT, DEFAULT_LOCAL_HTTP_PROXY_PORT);
}
