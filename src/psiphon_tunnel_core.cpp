/*
* Copyright (c) 2020, Psiphon Inc.
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
#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")

#include "psiphon_tunnel_core.h"
#include "diagnostic_info.h"
#include "logging.h"
#include "psiclient.h"
#include "utilities.h"


PsiphonTunnelCore::PsiphonTunnelCore(IPsiphonTunnelCoreNoticeHandler* noticeHandler, const tstring& exePath)
    : Subprocess(exePath, this),
      m_panicked(false)
{
    if (noticeHandler == NULL) {
        throw std::exception(__FUNCTION__ ":" STRINGIZE(__LINE__) "noticeHandler null");
    }
    m_noticeHandler = noticeHandler;
}


PsiphonTunnelCore::~PsiphonTunnelCore()
{
}

void PsiphonTunnelCore::HandleSubprocessOutputLine(const string& line)
{
    // Notices are logged to diagnostics. Some notices are excluded from
    // diagnostics if they may contain private user data.
    bool logOutputToDiagnostics = true;

    // Parse output to extract data

    Json::Value notice;
    Json::Reader reader;
    if (!reader.parse(line, notice))
    {
        // If the line contains "panic" or "fatal error", assume the core is crashing and add all further output to diagnostics
        vector<string> PANIC_HEADERS = { "panic", "fatal error" };

        for (const auto& panic : PANIC_HEADERS) {
            if (line.find(panic) != string::npos) {
                m_panicked = true;
                break;
            }
        }

        if (m_panicked)
        {
            // We do not think that a panic will contain private data
            my_print(NOT_SENSITIVE, false, _T("core panic: %S"), line.c_str());
            AddDiagnosticInfoJson("CorePanic", line.c_str());
        }
        else
        {
            my_print(SENSITIVE_FORMAT_ARGS, false, _T("%s: core notice JSON parse failed: %S"), __TFUNCTION__, reader.getFormattedErrorMessages().c_str());
            // This line was not JSON. It's not included in diagnostics
            // as we can't be sure it doesn't include user private data.
        }

        return;
    }

    if (!notice.isObject())
    {
        // Ignore this line. The core internals may emit non-notice format
        // lines, and this confuses the JSON parser. This test filters out
        // those lines.
        return;
    }

    try
    {
        string noticeType = notice["noticeType"].asString();
        string timestamp = notice["timestamp"].asString();
        Json::Value data = notice["data"];

        // Let the UI know about it and decide if something needs to be shown to the user.
        if (noticeType != "Info")
        {
            UI_Notice(line);
        }

        // Ensure any sensitive notices are not logged.
        if (noticeType == "ClientUpgradeDownloaded")
        {
            // Don't include in diagnostics as "filename" field in notice data
            // is private user data
            logOutputToDiagnostics = false;
        }
        else if (noticeType == "Untunneled")
        {
            // Don't include in diagnostics as "address" field in notice data
            // is private user data
            logOutputToDiagnostics = false;
        }
        else if (noticeType == "UpstreamProxyError")
        {
            // Don't include in diagnostics as "message" field in notice data
            // may contain private user data
            logOutputToDiagnostics = false;
        }

        m_noticeHandler->HandlePsiphonTunnelCoreNotice(noticeType, timestamp, data);
    }
    catch (exception& e)
    {
        my_print(SENSITIVE_FORMAT_ARGS, false, _T("%s: core notice JSON parse exception: %S"), __TFUNCTION__, e.what());
        return;
    }

    // Debug output, flag sensitive to exclude from feedback
    my_print(SENSITIVE_LOG, true, _T("core notice: %S"), line.c_str());

    // Add to diagnostics
    if (logOutputToDiagnostics)
    {
        AddDiagnosticInfoJson("CoreNotice", line.c_str());
    }
}
