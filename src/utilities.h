/*
 * Copyright (c) 2012, Psiphon Inc.
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

struct StopInfo;


bool ExtractExecutable(DWORD resourceID, const TCHAR* exeFilename, tstring& path);

// Possible return values:
//  ERROR_SUCCESS on success
//  WAIT_TIMEOUT if timeout exceeded
//  ERROR_SYSTEM_PROCESS_TERMINATED if process died
//  ERROR_OPERATION_ABORTED if cancel event signaled
// process and cancelEvent can be NULL
DWORD WaitForConnectability(
        int port, 
        DWORD timeout, 
        HANDLE process, 
        const StopInfo& stopInfo);

// NOTE: targetPort is inout, outputing the first available port
bool TestForOpenPort(int& targetPort, int maxIncrement, const StopInfo& stopInfo);

void StopProcess(DWORD processID, HANDLE process);

enum RegistryFailureReason
{
    REGISTRY_FAILURE_NO_REASON = 0,
    REGISTRY_FAILURE_WRITE_TOO_LONG
};

bool WriteRegistryDwordValue(const string& name, DWORD value);
bool ReadRegistryDwordValue(const string& name, DWORD& value);
bool WriteRegistryStringValue(const string& name, const string& value, RegistryFailureReason& reason);
bool ReadRegistryStringValue(LPCSTR name, string& value);
bool ReadRegistryStringValue(LPCWSTR name, wstring& value);

// Text metrics are relative to default font

int TextHeight(void);

int TextWidth(const TCHAR* text);

int LongestTextWidth(const TCHAR* texts[], int count);

// Returns true if at least one item in the array is true.
bool TestBoolArray(const vector<const bool*>& boolArray);

string Hexlify(const unsigned char* input, size_t length);

string Dehexlify(const string& input);

tstring GetLocaleName();

tstring GetISO8601DatetimeString();

bool PublicKeyEncryptData(const char* publicKey, const char* plaintext, string& o_encrypted);

DWORD GetTickCountDiff(DWORD start, DWORD end);

/*
String Utilities
*/

// Adapted from http://stackoverflow.com/questions/236129/splitting-a-string-in-c

template <typename charT>
vector<basic_string<charT>>& split(const basic_string<charT> &s, charT delim, std::vector<basic_string<charT>> &elems) {
    basic_stringstream<charT> ss(s);
    basic_string<charT> item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

template <typename charT>
std::vector<basic_string<charT>> split(const basic_string<charT> &s, charT delim) {
    vector<basic_string<charT>> elems;
    return split(s, delim, elems);
}
