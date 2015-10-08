/*
* Copyright (c) 2015, Psiphon Inc.
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
#include "utilities.h"
#include "psiclient.h"
#include "logging.h"


/*
## Regarding notices, logs, feedback

* `UI_Notice` should be called for: a) messages that need to be shown to user 
  in a modal, or b) high-priority messages that can go in the log but should be 
  translated (they may or may not be actionable, but aren't fatal enough to 
  show in a modal).
    - Does not have sensitivity flag, since it's not added to feedback.
* `my_print` is displayed to user (mid-priority) and is added to 
  `MessageHistory` (and so feedback).
    - Has sensitivity flag.
* `AddDiagnosticInfoJson` is added to `DiagnosticHistory` (and so feedback) but 
  is not displayed to user.
    - Does not have sensitivity flag. It's only called with data that needs to go in feedback.

In some cases it may be desirable to pair `UI_Notice` with one of the other 
calls, to get information to the user and into feedback.

There is a lot of overlap in meaning between `MessageHistory` and 
`DiagnosticHistory` and maybe they should be merged.
*/

// Logging messages will be posted to main window
extern HWND g_hWnd;


//==== my_print (logging) =====================================================

vector<MessageHistoryEntry> g_messageHistory;
HANDLE g_messageHistoryMutex = CreateMutex(NULL, FALSE, 0);

void GetMessageHistory(vector<MessageHistoryEntry>& history)
{
    AutoMUTEX mutex(g_messageHistoryMutex);
    history = g_messageHistory;
}

void AddMessageEntryToHistory(
    LogSensitivity sensitivity,
    bool bDebugMessage,
    const TCHAR* formatString,
    const TCHAR* finalString)
{
    AutoMUTEX mutex(g_messageHistoryMutex);

    const TCHAR* historicalMessage = NULL;
    if (sensitivity == NOT_SENSITIVE)
    {
        historicalMessage = finalString;
    }
    else if (sensitivity == SENSITIVE_FORMAT_ARGS)
    {
        historicalMessage = formatString;
    }
    else // SENSITIVE_LOG
    {
        historicalMessage = NULL;
    }

    if (historicalMessage != NULL)
    {
        MessageHistoryEntry entry;
        entry.message = historicalMessage;
        entry.timestamp = GetISO8601DatetimeString();
        entry.debug = bDebugMessage;
        g_messageHistory.push_back(entry);
    }
}

#ifdef _DEBUG
bool g_bShowDebugMessages = true;
#else
bool g_bShowDebugMessages = false;
#endif

void my_print(LogSensitivity sensitivity, bool bDebugMessage, const TCHAR* format, ...)
{
    TCHAR* debugPrefix = _T("DEBUG: ");
    size_t debugPrefixLength = _tcsclen(debugPrefix);
    TCHAR* buffer = NULL;
    va_list args;
    va_start(args, format);
    int length = _vsctprintf(format, args) + 1;
    if (bDebugMessage)
    {
        length += debugPrefixLength;
    }
    buffer = (TCHAR*)malloc(length * sizeof(TCHAR));
    if (!buffer) return;
    if (bDebugMessage)
    {
        _tcscpy_s(buffer, length, debugPrefix);
        _vstprintf_s(buffer + debugPrefixLength, length - debugPrefixLength, format, args);
    }
    else
    {
        _vstprintf_s(buffer, length, format, args);
    }
    va_end(args);

    AddMessageEntryToHistory(sensitivity, bDebugMessage, format, buffer);

    if (!bDebugMessage || g_bShowDebugMessages)
    {
        // NOTE:
        // Main window handles displaying the message. This avoids
        // deadlocks with SendMessage. Main window will deallocate
        // buffer.

        PostMessage(g_hWnd, WM_PSIPHON_MY_PRINT, bDebugMessage ? 0 : 1, (LPARAM)buffer);
    }
}

void my_print(LogSensitivity sensitivity, bool bDebugMessage, const string& message)
{
    my_print(sensitivity, bDebugMessage, UTF8ToWString(message).c_str());
}
