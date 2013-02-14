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

// YAML-CPP produces some annoying warnings. 
// (And... disabling C4996 doesn't actually seem to work...)
#pragma warning(push)
#pragma warning(disable: 4146 4996)
#include "yaml-cpp/yaml.h"
#pragma warning(pop)


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


// Forward declarations. Do not access directly.
extern vector<string> g_diagnosticInfo;
void _AddDiagnosticInfoHelper(const char* entry);

/**
`message` is the identifier for this entry.
`entry` can be of any type that can a YAML::Emitter can handle. This includes
most primitive types, std::string, std::map, std::vector, std::list.
Note that it does *not* include std::wstring (or tstring).
Other approaches for custom data types can be seen here:
http://code.google.com/p/yaml-cpp/wiki/HowToEmitYAML#STL_Containers,_and_Other_Overloads
and here:
http://code.google.com/p/yaml-cpp/wiki/Tutorial#Converting_To/From_Native_Data_Types
*/
template<typename T>
void AddDiagnosticInfo(const char* message, const T& entry)
{
    YAML::Emitter out;
    out.SetOutputCharset(YAML::EscapeNonAscii);
    out << YAML::BeginMap;
    out << YAML::Key << "timestamp" << YAML::Value << TStringToNarrow(GetISO8601DatetimeString()).c_str();
    out << YAML::Key << "msg" << YAML::Value << message;
    out << YAML::Key << "data" << entry;
    out << YAML::EndMap;
    _AddDiagnosticInfoHelper(out.c_str());
}

/**
`message` is the identifier for this entry.
`yaml` can be any valid YAML string. Note that this allows for easy single-entry
maps: "key: value". And for multi-entry maps: "{key1: val1, key2: val2}".
*/
void AddDiagnosticInfoYaml(const char* message, const char* yaml);
