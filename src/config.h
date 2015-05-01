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

#include <tchar.h>

static const TCHAR* INFO_LINK_PROMPT = _T("About Psiphon 3");
static const TCHAR* LOCAL_SETTINGS_APPDATA_SUBDIRECTORY = _T("Psiphon3");
static const TCHAR* LOCAL_SETTINGS_APPDATA_CONFIG_FILENAME = _T("psiphon.config");
static const TCHAR* LOCAL_SETTINGS_APPDATA_URL_PROXY_CONFIG_FILENAME = _T("url_proxy.config");
static const TCHAR* LOCAL_SETTINGS_APPDATA_SERVER_LIST_FILENAME = _T("server_list.dat");
static const TCHAR* LOCAL_SETTINGS_REGISTRY_KEY = _T("Software\\Psiphon3");
static const char* LOCAL_SETTINGS_REGISTRY_VALUE_SERVERS = "Servers";
static const char* LOCAL_SETTINGS_REGISTRY_VALUE_LAST_CONNECTED = "LastConnected";
static const char* LOCAL_SETTINGS_REGISTRY_VALUE_NATIVE_PROXY_INFO = "NativeProxyInfo";
static const char* LOCAL_SETTINGS_REGISTRY_VALUE_PSIPHON_PROXY_INFO = "PsiphonProxyInfo";
static const char* CLIENT_PLATFORM = "Windows";
static const TCHAR* HTTP_HANDSHAKE_REQUEST_PATH = _T("/handshake");
static const TCHAR* HTTP_CONNECTED_REQUEST_PATH = _T("/connected");
static const TCHAR* HTTP_STATUS_REQUEST_PATH = _T("/status");
static const TCHAR* HTTP_FAILED_REQUEST_PATH = _T("/failed");
static const TCHAR* HTTP_DOWNLOAD_REQUEST_PATH = _T("/download");
static const int CLIENT_SESSION_ID_BYTES = 16;
static const int DEFAULT_LOCAL_HTTP_PROXY_PORT = 8080;
static const int SECONDS_BETWEEN_SUCCESSFUL_REMOTE_SERVER_LIST_FETCH = 60*60*6;
static const int SECONDS_BETWEEN_UNSUCCESSFUL_REMOTE_SERVER_LIST_FETCH = 60*5;
static const int HTTPS_REQUEST_CONNECT_TIMEOUT_MS = 30000;
static const int HTTPS_REQUEST_SEND_TIMEOUT_MS = 30000;
static const int HTTPS_REQUEST_RECEIVE_TIMEOUT_MS = 30000;
static const int TERMINATE_PROCESS_WAIT_MS = 5000;
static const char* UNTUNNELED_WEB_REQUEST_CAPABILITY = "handshake";
static const int TEMPORARY_TUNNEL_TIMEOUT_SECONDS = 20;
static const int TARGET_PROTOCOL_ROTATION_SESSION_DURATION_THRESHOLD_MS = 2*60*1000;
static const int TARGET_PROTOCOL_ROTATION_PERIOD_MS = 30*1000;
