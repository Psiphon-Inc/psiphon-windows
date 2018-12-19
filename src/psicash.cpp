#include "stdafx.h"
#include "psicash.h"
#include "3rdParty\psicash\psicash.hpp"
#include "httpsrequest.h"
#include "utilities.h"
#include "config.h"


namespace psicash {

static constexpr bool TESTING = true;
static constexpr auto USER_AGENT = "Psiphon-PsiCash-iOS"; // TODO: Update for Windows

psicash::HTTPResult MakeHTTPRequest(const psicash::HTTPParams& params);

Lib::Lib() {
    m_mutex = CreateMutex(NULL, FALSE, 0);
}

Lib::~Lib() {
    CloseHandle(m_mutex);
}

error::Error Lib::Init() {
    if (m_initialized) {
        return error::nullerr;
    }

    AutoMUTEX lock(m_mutex);

    tstring dataDir;
    if (!GetDataPath({ LOCAL_SETTINGS_APPDATA_SUBDIRECTORY, _T("psicash") }, dataDir)) {
        return MakeError("GetDataPath failed");
    }

    // C++'s standard library doesn't deal with wide strings, so make the path
    // short and narrow.
    tstring dataDirShort;
    if (!GetShortPathName(dataDir, dataDirShort)) {
        return MakeError("GetShortPathName failed");
    }

    auto err = PsiCash::Init(USER_AGENT, WStringToNarrow(dataDirShort).c_str(), MakeHTTPRequest, TESTING);
    if (err) {
        return WrapError(err, "m_psicash.Init failed");
    }

    return error::nullerr;
}


psicash::HTTPResult MakeHTTPRequest(const psicash::HTTPParams& params) {
    // NOTE: This makes only HTTPS requests and ignores params.scheme

    wstringstream requestPath;
    requestPath << UTF8ToWString(params.path);
    if (!params.query.empty()) {
        requestPath << L"?";
    }
    for (auto qp : params.query) {
        requestPath << UTF8ToWString(qp.first) << L"=" << UTF8ToWString(qp.second);
    }

    wstringstream headers;
    for (auto header : params.headers) {
        headers << UTF8ToWString(header.first) << L": " << UTF8ToWString(header.second) << L"\r\n";
    }

    HTTPResult result;

    HTTPSRequest httpsRequest;
    HTTPSRequest::Response httpsResponse;
    if (!httpsRequest.MakeRequest(
            UTF8ToWString(params.hostname).c_str(),
            params.port,
            "",         // webServerCertificate
            requestPath.str().c_str(),
            httpsResponse,
            StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_EXIT),
            true,       // usePsiphonLocalProxy
            true,       // failoverToURLProxy
            headers.str().empty() ? NULL : headers.str().c_str(),
            NULL,       // additionalData
            0,          // additionalDataLength
            UTF8ToWString(params.method).c_str())) {
        result.error = "httpsRequest.MakeRequest failed";
        return result;
    }

    result.code = httpsResponse.code;
    result.body = httpsResponse.body;
    result.date = httpsResponse.dateHeader;
    return std::move(result);
}


} // namespace psicash