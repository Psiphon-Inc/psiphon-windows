/*
 * Copyright (c) 2013, Psiphon Inc.
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


/**
Should be called before Psiphon has attempted to connect or made any system
network changes.
*/
void DoStartupDiagnosticCollection();


bool SendFeedbackAndDiagnosticInfo(
        const string& feedback, 
        const string& emailAddress,
        const string& surveyJSON,
        bool sendDiagnosticInfo, 
        const StopInfo& stopInfo);


// Forward declarations. Do not access directly. (They're only here because the
// template function needs them.)
extern vector<string> g_diagnosticInfo;
void _AddDiagnosticInfoHelper(const char* entry);


/**
`message` is the identifier for this entry.
`jsonValue` is a JSON value. `jsonString` is a stringified JSON value.
`jsonString` may be null if no value is desired.
*/
void AddDiagnosticInfoJson(const char* message, const Json::Value& jsonValue);
void AddDiagnosticInfoJson(const char* message, const char* jsonString);


/**
`message` is the identifier for this entry.
`entry` can be of any type that can a Json::Value can handle -- see docs:
https://open-source-parsers.github.io/jsoncpp-docs/doxygen/class_json_1_1_value.html
*/
template<typename T>
void AddDiagnosticInfo(const char* message, const T& entry)
{
    Json::Value json(Json::objectValue);
    json["timestamp!!timestamp"] = WStringToUTF8(GetISO8601DatetimeString());
    json["msg"] = message;
    json["data"] = entry;

    Json::FastWriter jsonWriter;
    string jsonString = jsonWriter.write(json);

    OutputDebugStringA(jsonString.c_str());
    OutputDebugStringA("\n");

    AutoMUTEX mutex(g_diagnosticHistoryMutex);
    g_diagnosticHistory.append(json);
}


// 
// Utilities
// Some diagnostic info is useful outside of feedback
//

bool GetCountryDialingCode(wstring& o_countryDialingCode);
