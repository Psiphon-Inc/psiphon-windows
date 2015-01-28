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

    void Show(HINSTANCE hInst, HWND hParentWnd);

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

    bool SkipBrowser();
    bool SkipProxySettings();
}
