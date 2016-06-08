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


/*
 * File Utilities
 */

bool ExtractExecutable(
    DWORD resourceID,
    const TCHAR* exeFilename,
    tstring& path,
    bool succeedIfExists=false);

bool GetShortPathName(const tstring& path, tstring& o_shortPath);

bool WriteFile(const tstring& filename, const string& data);

bool GetTempPath(tstring& o_path);

// Makes an absolute path to a unique temp directory.
// If `create` is true, the directory will also be created.
// Returns true on success, false otherwise. Caller can check GetLastError() on failure.
bool GetUniqueTempDir(tstring& o_path, bool create);


/*
 * Network and IPC Utilities
 */

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

bool CreateSubprocessPipes(
        HANDLE& o_parentOutputPipe, // Parent reads the child's stdout/stdin from this
        HANDLE& o_parentInputPipe,  // Parent writes to the child's stdin with this
        HANDLE& o_childStdinPipe,   // Child's stdin pipe
        HANDLE& o_childStdoutPipe,  // Child's stdout pipe
        HANDLE& o_childStderrPipe);  // Child's stderr pipe (dup of stdout)


/*
 * Registry Utilities
 */

enum RegistryFailureReason
{
    REGISTRY_FAILURE_NO_REASON = 0,
    REGISTRY_FAILURE_WRITE_TOO_LONG
};

bool WriteRegistryDwordValue(const string& name, DWORD value);
bool ReadRegistryDwordValue(const string& name, DWORD& value);
bool WriteRegistryStringValue(const string& name, const string& value, RegistryFailureReason& reason);
bool WriteRegistryStringValue(const string& name, const wstring& value, RegistryFailureReason& reason);
bool ReadRegistryStringValue(LPCSTR name, string& value);
bool ReadRegistryStringValue(LPCSTR name, wstring& value);


/*
 * Text Display Utilities
 */

// Text metrics are relative to default font

int TextHeight(void);

int TextWidth(const TCHAR* text);

int LongestTextWidth(const TCHAR* texts[], int count);

// Returns true if at least one item in the array is true.
bool TestBoolArray(const vector<const bool*>& boolArray);


/*
 * Data Encoding Utilities
 */

string Hexlify(const unsigned char* input, size_t length);

string Dehexlify(const string& input);

string Base64Encode(const unsigned char* input, size_t length);
string Base64Decode(const string& input);

bool PublicKeyEncryptData(const char* publicKey, const char* plaintext, string& o_encrypted);

tstring UrlEncode(const tstring& input);
tstring UrlDecode(const tstring& input);


/*
 * System Utilities
 */

DWORD GetTickCountDiff(DWORD start, DWORD end);

tstring GetLocaleName();

// Should be called (by psiclient) when the UI locale is set.
// (This is to help GetDeviceRegion().)
void SetUiLocale(const wstring& uiLocale);

// Makes best guess as the country that the application/device is currently 
// running in. Returns ISO 3166-1 alpha-2 format.
wstring GetDeviceRegion();

/*
 * Miscellaneous Utilities
 */

tstring GetISO8601DatetimeString();

// Makes a GUID string. Returns true on success, false otherwise.
bool MakeGUID(tstring& o_guid);


/*
 * String Utilities
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

#ifndef STRINGIZE
// From MSVC++ 2012's _STRINGIZE macro
#define __STRINGIZEX(x) #x
#define STRINGIZE(x) __STRINGIZEX(x)
#endif


/*
 * Resource Utilities
 */

// Returns success. o_pBytes points to resource data; o_size is the size of that data.
bool GetResourceBytes(DWORD name, DWORD type, BYTE*& o_pBytes, DWORD& o_size);
bool GetResourceBytes(DWORD name, LPCTSTR type, BYTE*& o_pBytes, DWORD& o_size);
bool GetResourceBytes(LPCTSTR name, DWORD type, BYTE*& o_pBytes, DWORD& o_size);
bool GetResourceBytes(LPCTSTR name, LPCTSTR type, BYTE*& o_pBytes, DWORD& o_size);


/*
 * AutoHANDLE and AutoMUTEX
 */
class AutoHANDLE
{
public:
    AutoHANDLE(HANDLE handle) { m_handle = handle; }
    ~AutoHANDLE() { CloseHandle(m_handle); }
    operator HANDLE() { return m_handle; }
private:
    HANDLE m_handle;
};

class AutoMUTEX
{
public:
    AutoMUTEX(HANDLE mutex, TCHAR* logInfo = 0);
    ~AutoMUTEX();
private:
    HANDLE m_mutex;
    tstring m_logInfo;
};

#define AUTOMUTEX(mutex)


/*
finally function

Pass a (lambda) function to finally(...) to ensure it will be executed when 
leaving the current scope. E.g.,
char* d = new char[...];
auto deleteOnReturn = finally([d] { delete[] d; });

From http://stackoverflow.com/a/25510879/729729
*/

template <typename F>
struct FinalAction {
    FinalAction(F f) : clean_{ f } {}
    ~FinalAction() { clean_(); }
    F clean_;
};

template <typename F>
FinalAction<F> finally(F f) {
    return FinalAction<F>(f);
}


/*
 * DPI Awareness Utilities
 */

// From ShellScalingAPI.h
#ifndef DPI_ENUMS_DECLARED
typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;
typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;
#define DPI_ENUMS_DECLARED
#endif // (DPI_ENUMS_DECLARED)
#ifndef WM_DPICHANGED
#define WM_DPICHANGED       0x02E0
#endif

// Unlike the real version of this, it will return ERROR_NOT_SUPPORTED on OS 
// versions that do not support it.
HRESULT SetProcessDpiAwareness(PROCESS_DPI_AWARENESS value);

// Helper for getting the useful DPI value. 
// Returns ERROR_NOT_SUPPORTED on OS versions that do not support it.
HRESULT GetDpiForCurrentMonitor(HWND hWnd, UINT& o_dpi);

// Helper for getting the useful DPI scaling value. 
// o_scale will be like 1.0, 1.25, 1.5, 2.0, 2.5
// Returns ERROR_NOT_SUPPORTED on OS versions that do not support it.
HRESULT GetDpiScalingForCurrentMonitor(HWND hWnd, float& o_scale);

// Finds the DPI scaling factor for the monitor at point pt.
// o_scale will be like 1.0, 1.25, 1.5, 2.0, 2.5
// Returns ERROR_NOT_SUPPORTED on OS versions that do not support it.
HRESULT GetDpiScalingForMonitorFromPoint(POINT pt, float& o_scale);

// Helper for converting DPI value to scaling value.
float ConvertDpiToScaling(UINT dpi);
