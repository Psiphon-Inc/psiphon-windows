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


namespace Settings
{
    void Initialize();

    void ToJson(Json::Value& o_json);
    // Returns false on error. o_reconnectRequired will be true if a reconnect
    // is going to occur to apply the settings.
    bool FromJson(const string& utf8JSON, bool& o_reconnectRequired);

    // Returns true if settings changed.
    bool Show(HINSTANCE hInst, HWND hParentWnd);

    bool SplitTunnel();
    tstring Transport();
    
    // Returns 0 if port should be chosen automatically.
    unsigned int LocalHttpProxyPort();
    // Returns 0 if port should be chosen automatically.
    unsigned int LocalSocksProxyPort();

    bool SkipUpstreamProxy();
    string UpstreamProxyType();
    string UpstreamProxyHostname();
    unsigned int UpstreamProxyPort();

    string EgressRegion();

    bool SystrayMinimize();

    bool SkipBrowser();
    bool SkipProxySettings();
    bool SkipAutoConnect();

    // These are used by the web UI
    void SetCookies(const string& value);
    string GetCookies();

    // Used for storing and restoring the main window placement
    void SetWindowPlacement(const string& value);
    string GetWindowPlacement();
}
