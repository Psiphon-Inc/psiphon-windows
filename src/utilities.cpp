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


#include "stdafx.h"
#include "psiclient.h"
#include "config.h"
#include <Shlwapi.h>
#include <WinSock2.h>
#include <TlHelp32.h>
#include "utilities.h"
#include "stopsignal.h"
#include "cryptlib.h"
#include "cryptlib.h"
#include "rsa.h"
#include "base64.h"
#include "osrng.h"
#include "modes.h"
#include "hmac.h"


extern HINSTANCE g_hInst;

// Adapted from here:
// http://stackoverflow.com/questions/865152/how-can-i-get-a-process-handle-by-its-name-in-c
void TerminateProcessByName(const TCHAR* executableName)
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(snapshot, &entry))
    {
        do
        {
            if (_tcsicmp(entry.szExeFile, executableName) == 0)
            {
                HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
                if (!TerminateProcess(process, 0) ||
                    WAIT_OBJECT_0 != WaitForSingleObject(process, TERMINATE_PROCESS_WAIT_MS))
                {
                    my_print(false, _T("TerminateProcess failed for process with name %s"), executableName);
                    my_print(false, _T("Please terminate this process manually"));
                }
                CloseHandle(process);
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
}


bool ExtractExecutable(DWORD resourceID, const TCHAR* exeFilename, tstring& path)
{
    // Extract executable from resources and write to temporary file

    HRSRC res;
    HGLOBAL handle = INVALID_HANDLE_VALUE;
    BYTE* data;
    DWORD size;

    res = FindResource(g_hInst, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    if (!res)
    {
        my_print(false, _T("ExtractExecutable - FindResource failed (%d)"), GetLastError());
        return false;
    }

    handle = LoadResource(NULL, res);
    if (!handle)
    {
        my_print(false, _T("ExtractExecutable - LoadResource failed (%d)"), GetLastError());
        return false;
    }

    data = (BYTE*)LockResource(handle);
    size = SizeofResource(NULL, res);

    DWORD ret;
    TCHAR tempPath[MAX_PATH];
    // http://msdn.microsoft.com/en-us/library/aa364991%28v=vs.85%29.aspx notes
    // tempPath can contain no more than MAX_PATH-14 characters
    ret = GetTempPath(MAX_PATH, tempPath);
    if (ret > MAX_PATH-14 || ret == 0)
    {
        my_print(false, _T("ExtractExecutable - GetTempPath failed (%d)"), GetLastError());
        return false;
    }

    TCHAR filePath[MAX_PATH];
    if (NULL == PathCombine(filePath, tempPath, exeFilename))
    {
        my_print(false, _T("ExtractExecutable - PathCombine failed (%d)"), GetLastError());
        return false;
    }

    HANDLE tempFile = INVALID_HANDLE_VALUE;
    bool attemptedTerminate = false;
    while (true)
    {
        tempFile = CreateFile(filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (tempFile == INVALID_HANDLE_VALUE)
        {
            int lastError = GetLastError();
            if (!attemptedTerminate &&
                ERROR_SHARING_VIOLATION == lastError)
            {
                TerminateProcessByName(exeFilename);
                attemptedTerminate = true;
            }
            else
            {
                my_print(false, _T("ExtractExecutable - CreateFile failed (%d)"), lastError);
                return false;
            }
        }
        else
        {
            break;
        }
    }

    DWORD written = 0;
    if (!WriteFile(tempFile, data, size, &written, NULL)
        || written != size
        || !FlushFileBuffers(tempFile))
    {
        CloseHandle(tempFile);
        my_print(false, _T("ExtractExecutable - WriteFile/FlushFileBuffers failed (%d)"), GetLastError());
        return false;
    }

    CloseHandle(tempFile);

    path = filePath;

    return true;
}


DWORD WaitForConnectability(
        int port,
        DWORD timeout,
        HANDLE process,
        const StopInfo& stopInfo)
{
    // There are a number of options for monitoring the connected status
    // of plonk/polipo. We're going with a quick and dirty solution of
    // (a) monitoring the child processes -- if they exit, there was an error;
    // (b) asynchronously connecting to the plonk SOCKS server, which isn't
    //     started by plonk until its ssh tunnel is established.
    // Note: piping stdout/stderr of the child processes and monitoring
    // messages is problematic because we don't control the C I/O flushing
    // of these processes (http://support.microsoft.com/kb/190351).
    // Additional measures or alternatives include making actual HTTP
    // requests through the entire stack from time to time or switching
    // to integrated ssh/http libraries with APIs.

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(port);

    SOCKET sock = INVALID_SOCKET;
    WSAEVENT connectedEvent = WSACreateEvent();
    WSANETWORKEVENTS networkEvents;

    // Wait up to SSH_CONNECTION_TIMEOUT_SECONDS, checking periodically for user cancel

    DWORD start = GetTickCount();
    DWORD maxWaitMilliseconds = timeout;

    DWORD returnValue = ERROR_SUCCESS;

    while (true)
    {
        DWORD now = GetTickCount();

        if (now < start // Note: GetTickCount wraps after 49 days; small chance of a shorter timeout
            || now >= start + maxWaitMilliseconds)
        {
            returnValue = WAIT_TIMEOUT;
            break;
        }

        // Attempt to connect to SOCKS proxy
        // Just wait 100 ms. and then check for user cancel etc.

        closesocket(sock);
        sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (INVALID_SOCKET != sock
            && 0 == WSAEventSelect(sock, connectedEvent, FD_CONNECT)
            && SOCKET_ERROR == connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr))
            && WSAEWOULDBLOCK == WSAGetLastError()
            && WSA_WAIT_EVENT_0 == WSAWaitForMultipleEvents(1, &connectedEvent, TRUE, 100, FALSE)
            && 0 == WSAEnumNetworkEvents(sock, connectedEvent, &networkEvents)
            && (networkEvents.lNetworkEvents & FD_CONNECT)
            && networkEvents.iErrorCode[FD_CONNECT_BIT] == 0)
        {
            returnValue = ERROR_SUCCESS;
            break;
        }

        // If server aborted, give up

        if (process != NULL
            && WAIT_OBJECT_0 == WaitForSingleObject(process, 0))
        {
            returnValue = ERROR_SYSTEM_PROCESS_TERMINATED;
            break;
        }

        // Check if cancel is signalled

        if (stopInfo.stopSignal->CheckSignal(stopInfo.stopReasons))
        {
            returnValue = ERROR_OPERATION_ABORTED;
            break;
        }
    }

    closesocket(sock);
    WSACloseEvent(connectedEvent);
    WSACleanup();

    return returnValue;
}


bool TestForOpenPort(int& targetPort, int maxIncrement, const StopInfo& stopInfo)
{
    int maxPort = targetPort + maxIncrement;
    do
    {
        if (ERROR_SUCCESS != WaitForConnectability(targetPort, 100, 0, stopInfo))
        {
            return true;
        }
        my_print(false, _T("Localhost port %d is already in use."), targetPort);
    }
    while (++targetPort <= maxPort);

    return false;
}


void StopProcess(DWORD processID, HANDLE process)
{
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, processID);
    if (WAIT_OBJECT_0 != WaitForSingleObject(process, 100))
    {
        if (!TerminateProcess(process, 0) ||
            WAIT_OBJECT_0 != WaitForSingleObject(process, TERMINATE_PROCESS_WAIT_MS))
        {
            my_print(false, _T("TerminateProcess failed for process with PID %d"), processID);
        }
    }
}


bool WriteRegistryDwordValue(const string& name, DWORD value)
{
    HKEY key = 0;
    DWORD disposition = 0;
    DWORD bufferLength = sizeof(value);

    bool success =
        (ERROR_SUCCESS == RegCreateKeyEx(
                            HKEY_CURRENT_USER,
                            LOCAL_SETTINGS_REGISTRY_KEY,
                            0,
                            0,
                            0,
                            KEY_WRITE,
                            0,
                            &key,
                            &disposition) &&

         ERROR_SUCCESS == RegSetValueExA(
                            key,
                            name.c_str(),
                            0,
                            REG_DWORD,
                            (LPBYTE)&value,
                            bufferLength));
    RegCloseKey(key);

    return success;
}


bool ReadRegistryDwordValue(const string& name, DWORD& value)
{
    HKEY key = 0;
    DWORD bufferLength = sizeof(value);
    DWORD type;

    bool success =
        (ERROR_SUCCESS == RegOpenKeyEx(
                            HKEY_CURRENT_USER,
                            LOCAL_SETTINGS_REGISTRY_KEY,
                            0,
                            KEY_READ,
                            &key) &&

         ERROR_SUCCESS == RegQueryValueExA(
                            key,
                            name.c_str(),
                            0,
                            &type,
                            (LPBYTE)&value,
                            &bufferLength) &&

        type == REG_DWORD);

    RegCloseKey(key);

    return success;
}


bool WriteRegistryStringValue(const string& name, const string& value)
{
    HKEY key = 0;

    bool success =
        (ERROR_SUCCESS == RegCreateKeyEx(
                            HKEY_CURRENT_USER,
                            LOCAL_SETTINGS_REGISTRY_KEY,
                            0,
                            0,
                            0,
                            KEY_WRITE,
                            0,
                            &key,
                            0) &&
         ERROR_SUCCESS == RegSetValueExA(
                            key,
                            name.c_str(),
                            0,
                            REG_SZ,
                            (LPBYTE)value.c_str(),
                            value.length() + 1)); // Write the null terminator
    RegCloseKey(key);

    return success;
}


bool ReadRegistryStringValue(LPCSTR name, string& value)
{
    bool success = false;
    HKEY key = 0;
    DWORD bufferLength = 0;
    char* buffer = 0;
    DWORD type;

    if (ERROR_SUCCESS == RegOpenKeyEx(
                            HKEY_CURRENT_USER,
                            LOCAL_SETTINGS_REGISTRY_KEY,
                            0,
                            KEY_READ,
                            &key) &&

        ERROR_SUCCESS == RegQueryValueExA(
                            key,
                            name,
                            0,
                            0,
                            NULL,
                            &bufferLength) &&

        (buffer = new char[bufferLength + 1]) &&

        ERROR_SUCCESS == RegQueryValueExA(
                            key,
                            name,
                            0,
                            &type,
                            (LPBYTE)buffer,
                            &bufferLength) &&
        type == REG_SZ)
    {
        buffer[bufferLength] = '\0';
        value = buffer;
        success = true;
    }

    delete[] buffer;
    RegCloseKey(key);

    return success;
}

bool ReadRegistryStringValue(LPCWSTR name, wstring& value)
{
    bool success = false;
    HKEY key = 0;
    DWORD bufferLength = 0;
    wchar_t* buffer = 0;
    DWORD type;

    if (ERROR_SUCCESS == RegOpenKeyEx(
                            HKEY_CURRENT_USER,
                            LOCAL_SETTINGS_REGISTRY_KEY,
                            0,
                            KEY_READ,
                            &key) &&

        ERROR_SUCCESS == RegQueryValueExW(
                            key,
                            name,
                            0,
                            0,
                            NULL,
                            &bufferLength) &&

        (buffer = new wchar_t[bufferLength + 1]) &&

        ERROR_SUCCESS == RegQueryValueExW(
                            key,
                            name,
                            0,
                            &type,
                            (LPBYTE)buffer,
                            &bufferLength) &&
        type == REG_SZ)
    {
        buffer[bufferLength] = '\0';
        value = buffer;
        success = true;
    }

    delete[] buffer;
    RegCloseKey(key);

    return success;
}


int TextHeight(void)
{
    HWND hWnd = CreateWindow(L"Static", 0, 0, 0, 0, 0, 0, 0, 0, g_hInst, 0);
    HGDIOBJ font = GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hWnd, WM_SETFONT, (WPARAM)font, NULL);
    TEXTMETRIC textMetric;
    BOOL success = GetTextMetrics(GetDC(hWnd), &textMetric);
    DestroyWindow(hWnd);
    return success ? textMetric.tmHeight : 0;
}


int TextWidth(const TCHAR* text)
{
    HWND hWnd = CreateWindow(L"Static", 0, 0, 0, 0, 0, 0, 0, 0, g_hInst, 0);
    HGDIOBJ font = GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(hWnd, WM_SETFONT, (WPARAM)font, NULL);
    SIZE size;
    BOOL success = GetTextExtentPoint32(GetDC(hWnd), text, _tcslen(text), &size);
    DestroyWindow(hWnd);
    return success ? size.cx : 0;
}


int LongestTextWidth(const TCHAR* texts[], int count)
{
    int longestWidth = 0;
    for (int i = 0; i < count; i++)
    {
        int width = TextWidth(texts[i]);
        if (width > longestWidth)
        {
            longestWidth = width;
        }
    }
    return longestWidth;
}


bool TestBoolArray(const vector<const bool*>& boolArray)
{
    for (size_t i = 0; i < boolArray.size(); i++)
    {
        if (*(boolArray[i]))
        {
            return true;
        }
    }

    return false;
}

// Adapted from here:
// http://stackoverflow.com/questions/3381614/c-convert-string-to-hexadecimal-and-vice-versa
string Hexlify(const unsigned char* input, size_t length)
{
    static const char* const lut = "0123456789ABCDEF";

    string output;
    output.reserve(2 * length);
    for (size_t i = 0; i < length; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

string Dehexlify(const string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();
    if (len & 1)
    {
        throw std::invalid_argument("Dehexlify: odd length");
    }

    string output;
    output.reserve(len / 2);
    for (size_t i = 0; i < len; i += 2)
    {
        char a = toupper(input[i]);
        const char* p = std::lower_bound(lut, lut + 16, a);
        if (*p != a)
        {
            throw std::invalid_argument("Dehexlify: not a hex digit");
        }

        char b = toupper(input[i + 1]);
        const char* q = std::lower_bound(lut, lut + 16, b);
        if (*q != b)
        {
            throw std::invalid_argument("Dehexlify: not a hex digit");
        }

        output.push_back(((p - lut) << 4) | (q - lut));
    }

    return output;
}


tstring GetLocaleName()
{
    int size = GetLocaleInfo(
                LOCALE_USER_DEFAULT,
                LOCALE_SISO639LANGNAME,
                NULL,
                0);

    if (size <= 0)
    {
        return _T("");
    }

    LPTSTR buf = new TCHAR[size];

    size = GetLocaleInfo(
                LOCALE_USER_DEFAULT,
                LOCALE_SISO639LANGNAME,
                buf,
                size);

    if (size <= 0)
    {
        return _T("");
    }

    tstring ret = buf;

    delete[] buf;

    return ret;
}


tstring GetISO8601DatetimeString()
{
    SYSTEMTIME systime;
    GetSystemTime(&systime);

    TCHAR ret[64];
    _sntprintf_s(
        ret,
        sizeof(ret)/sizeof(ret[0]),
        _T("%04d-%02d-%02dT%02d:%02d:%02d.%03dZ"),
        systime.wYear,
        systime.wMonth,
        systime.wDay,
        systime.wHour,
        systime.wMinute,
        systime.wSecond,
        systime.wMilliseconds);

    return ret;
}


/*
 * Feedback Encryption
 */

bool PublicKeyEncryptData(const char* publicKey, const char* plaintext, string& o_encrypted)
{
    o_encrypted.clear();

    CryptoPP::AutoSeededRandomPool rng;

    string b64Ciphertext, b64Mac, b64WrappedEncryptionKey, b64WrappedMacKey, b64IV;

    try
    {
        string ciphertext, mac, wrappedEncryptionKey, wrappedMacKey;

        // NOTE: We are doing encrypt-then-MAC.

        // CryptoPP::AES::MIN_KEYLENGTH is 128 bits.
        int KEY_LENGTH = CryptoPP::AES::MIN_KEYLENGTH;

        //
        // Encrypt
        //

        CryptoPP::SecByteBlock encryptionKey(KEY_LENGTH);
        rng.GenerateBlock(encryptionKey, encryptionKey.size());

        byte iv[CryptoPP::AES::BLOCKSIZE];
        rng.GenerateBlock(iv, CryptoPP::AES::BLOCKSIZE);

        CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption encryptor;
        encryptor.SetKeyWithIV(encryptionKey, encryptionKey.size(), iv);

        CryptoPP::StringSource(
            plaintext,
            true,
            new CryptoPP::StreamTransformationFilter(
                encryptor,
                new CryptoPP::StringSink(ciphertext),
                CryptoPP::StreamTransformationFilter::PKCS_PADDING));

        CryptoPP::StringSource(
            ciphertext,
            true,
            new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(b64Ciphertext),
                false));

        CryptoPP::StringSource(
            iv,
            sizeof(iv)*sizeof(iv[0]),
            true,
            new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(b64IV),
                false));

        //
        // HMAC
        //

        CryptoPP::SecByteBlock macKey(KEY_LENGTH);
        rng.GenerateBlock(macKey, macKey.size());

        CryptoPP::HMAC<CryptoPP::SHA256> hmac(macKey, macKey.size());

        CryptoPP::StringSource(
            ciphertext,
            true,
            new CryptoPP::HashFilter(
                hmac,
                new CryptoPP::StringSink(mac)));

        CryptoPP::StringSource(
            mac,
            true,
            new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(b64Mac),
                false));

        //
        // Wrap the keys
        //

        CryptoPP::RSAES_OAEP_SHA_Encryptor rsaEncryptor(
            CryptoPP::StringSource(
                publicKey,
                true,
                new CryptoPP::Base64Decoder()));

        CryptoPP::StringSource(
            encryptionKey.data(),
            encryptionKey.size(),
            true,
            new CryptoPP::PK_EncryptorFilter(
                rng,
                rsaEncryptor,
                new CryptoPP::StringSink(wrappedEncryptionKey)));

        CryptoPP::StringSource(
            macKey.data(),
            macKey.size(),
            true,
            new CryptoPP::PK_EncryptorFilter(
                rng,
                rsaEncryptor,
                new CryptoPP::StringSink(wrappedMacKey)));

        CryptoPP::StringSource(
            wrappedEncryptionKey,
            true,
            new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(b64WrappedEncryptionKey),
                false));

        CryptoPP::StringSource(
            wrappedMacKey,
            true,
            new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(b64WrappedMacKey),
                false));
    }
    catch( const CryptoPP::Exception& e )
    {
        my_print(false, _T("%s - Encryption failed (%d): %S"), __TFUNCTION__, GetLastError(), e.what());
        return false;
    }

    stringstream ss;
    ss << "{  \n";
    ss << "  \"contentCiphertext\": \"" << b64Ciphertext << "\",\n";
    ss << "  \"iv\": \"" << b64IV << "\",\n";
    ss << "  \"wrappedEncryptionKey\": \"" << b64WrappedEncryptionKey << "\",\n";
    ss << "  \"contentMac\": \"" << b64Mac << "\",\n";
    ss << "  \"wrappedMacKey\": \"" << b64WrappedMacKey << "\"\n";
    ss << "}";

    o_encrypted = ss.str();

    return true;
}
