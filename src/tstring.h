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
#define tistringstream wistringstream
#define tregex wregex
#else
#define tstring string
#define tistringstream istringstream
#define tregex regex
#endif

typedef basic_stringstream<TCHAR> tstringstream;

static string WStringToUTF8(LPCWSTR wString)
{
    wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    const string utf8_string = converter.to_bytes(wString);
    return utf8_string;
}

static string WStringToUTF8(const wstring& wString)
{
    return WStringToUTF8(wString.c_str());
}

static wstring UTF8ToWString(LPCSTR utf8String)
{    
    // There is an issue in VS2015 that messes up codecvt. For a bit of info
    // and the workaround, see here:
    // https://social.msdn.microsoft.com/Forums/en-US/8f40dcd8-c67f-4eba-9134-a19b9178e481/vs-2015-rc-linker-stdcodecvt-error?forum=vcgeneral
    // These two lines are the correct (and currently broken) form:
    //std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> utf16conv;
    //std::u16string utf16 = utf16conv.from_bytes(utf8String);
    // And these two lines are the workaround:
    std::wstring_convert<std::codecvt_utf8_utf16<__int16>, __int16> utf16conv;
    basic_string<__int16, char_traits<__int16>, allocator<__int16> > utf16 = utf16conv.from_bytes(utf8String);

    wstring wide_string(utf16.begin(), utf16.end());
    return wide_string;
}

static wstring UTF8ToWString(const string& utf8String)
{
    return UTF8ToWString(utf8String.c_str());
}

// This function is used to handle UTF-8 encoded data stored inside of a wstring
static string WStringToNarrow(const wstring& wString)
{
    return string(wString.begin(), wString.end());
}

// This function is used to handle UTF-8 encoded data stored inside of a wstring
static wstring WidenUTF8(LPCTSTR utf8String)
{
#ifdef _UNICODE
    string narrowString = WStringToNarrow(utf8String);
#else
    string narrowString(utf8String);
#endif
    return UTF8ToWString(narrowString.c_str());
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
