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

static const char* PROPAGATION_CHANNEL_ID = 
    "3A885577DD84EF13";

static const char* SPONSOR_ID = 
    "8BB28C1A8E8A9ED9";

// NOTE: if we put this in resources instead/as well, it would show up
//       in Explorer properties tab, etc.
static const char* CLIENT_VERSION = "45";

static const char* EMBEDDED_SERVER_LIST = "";

// When this flag is set, only the embedded server list is used. This is for testing only.
static const int IGNORE_SYSTEM_SERVER_LIST = 0;

static const char* REMOTE_SERVER_LIST_SIGNATURE_PUBLIC_KEY = "";

static const char* FEEDBACK_ENCRYPTION_PUBLIC_KEY = "";

// These values are used when uploading diagnostic info
static const char* FEEDBACK_DIAGNOSTIC_INFO_UPLOAD_SERVER = "";
static const char* FEEDBACK_DIAGNOSTIC_INFO_UPLOAD_PATH = "";
static const char* FEEDBACK_DIAGNOSTIC_INFO_UPLOAD_SERVER_CERT = "";
static const char* FEEDBACK_DIAGNOSTIC_INFO_UPLOAD_SERVER_HEADERS = "";

static const char* REMOTE_SERVER_LIST_ADDRESS =
    "s3.amazonaws.com";

static const char* REMOTE_SERVER_LIST_REQUEST_PATH =
    "invalid_bucket_name/server_list";

// NOTE: Info link may be opened when not tunneled
static const TCHAR* INFO_LINK_URL
    = _T("https://sites.google.com/a/psiphon3.com/psiphon3/");
