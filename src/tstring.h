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
#include <sstream>

using namespace std;

#ifdef _UNICODE
#define tstring wstring
#else
#define tstring string
#endif

typedef basic_stringstream<TCHAR> tstringstream;

static tstring NarrowToTString(const string& narrowString)
{
#ifdef _UNICODE
    wstring wideString(narrowString.length(), L' ');
    std::copy(narrowString.begin(), narrowString.end(), wideString.begin());
    return wideString;
#else
    return narrowString;
#endif
}

static string TStringToNarrow(const tstring& tString)
{
#ifdef _UNICODE
    return string(tString.begin(), tString.end());
#else
    return tString;
#endif
}

static string WStringToNarrow(const wstring& wString)
{
    return string(wString.begin(), wString.end());
}

static string WStringToNarrow(LPCWSTR wString)
{
    return WStringToNarrow(wstring(wString));
}

#ifdef UNICODE
    #define WIDEN2(x) L##x
    #define WIDEN(x) WIDEN2(x)
    #define __WFILE__ WIDEN(__FILE__)
    #define __WFUNCTION__ WIDEN(__FUNCTION__)
    #define __TFILE__ __WFILE__
    #define __TFUNCTION__ __WFUNCTION__
#else
    #define __TFILE__ __FILE__
    #define __TFILE__ __FUNCTION__
#endif
