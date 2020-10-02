#include "stdafx.h"
#include "psicashlib.h"
#include "3rdParty\psicash\psicash.hpp"
#include "httpsrequest.h"
#include "utilities.h"
#include "config.h"
#include "logging.h"
#include "embeddedvalues.h"

using namespace std;

namespace psicash {

static constexpr bool TESTING = false;
static constexpr auto USER_AGENT = "Psiphon-PsiCash-Windows";

psicash::MakeHTTPRequestFn GetHTTPReqFn(const StopInfo& stopInfo);

Lib::Lib()
    : m_requestStopInfo(StopInfo(&GlobalStopSignal::Instance(), STOP_REASON_ANY_STOP_TUNNEL)),
      m_requestQueue("PsiCash request queue", 1) // we specifically only want one worker, for one request at a time
{
    m_mutex = CreateMutex(nullptr, FALSE, 0);
}

Lib::~Lib() {
    CloseHandle(m_mutex);
}

error::Error Lib::Init(bool forceReset) {
    AutoMUTEX lock(m_mutex);

    tstring dataDir;
    if (!GetPsiphonDataPath({ _T("psicash") }, true, dataDir)) {
        return psicash::error::MakeCriticalError("GetPsiphonDataPath failed");
    }

    // C++'s standard library doesn't deal with wide strings, so make the path
    // short and narrow.
    tstring dataDirShort;
    if (!GetShortPathName(dataDir, dataDirShort)) {
        return psicash::error::MakeCriticalError("GetShortPathName failed");
    }

    auto err = PsiCash::Init(
        USER_AGENT, WStringToNarrow(dataDirShort).c_str(), GetHTTPReqFn(m_requestStopInfo), forceReset, TESTING);
    if (err) {
        return WrapError(err, "PsiCash::Init failed");
    }

    err = PsiCash::SetRequestMetadataItem("client_version", CLIENT_VERSION);
    if (err) {
        return WrapError(err, "SetRequestMetadataItem failed");
    }
    err = PsiCash::SetRequestMetadataItem("propagation_channel_id", PROPAGATION_CHANNEL_ID);
    if (err) {
        return WrapError(err, "SetRequestMetadataItem failed");
    }
    err = PsiCash::SetRequestMetadataItem("sponsor_id", SPONSOR_ID);
    if (err) {
        return WrapError(err, "SetRequestMetadataItem failed");
    }
    err = PsiCash::SetRequestMetadataItem("client_region", WStringToUTF8(GetDeviceRegion()));
    if (err) {
        return WrapError(err, "SetRequestMetadataItem failed");
    }

    try { my_print(NOT_SENSITIVE, true, _T("%s: PsiCash state: %S"), __TFUNCTION__, PsiCash::GetDiagnosticInfo(true).dump(-1, ' ', true).c_str()); }
    catch (...) {}

    return error::nullerr;
}

error::Error Lib::UpdateClientRegion(const string& region) {
    auto err = PsiCash::SetRequestMetadataItem("client_region", region);
    if (err) {
        return WrapError(err, "SetRequestMetadataItem failed");
    }

    return error::nullerr;
}

enum class RequestType : int {
    RefreshState,
    NewExpiringPurchase,
    AccountLogin,
    AccountLogout
};

void Lib::RefreshState(
    bool local_only,
    std::function<void(error::Result<RefreshStateResponse>)> callback)
{
    // Don't queue this if there is already an outstanding RefreshState
    auto queued = m_requestQueue.dispatch((int)RequestType::RefreshState, { (int)RequestType::RefreshState }, [=] {
        callback(PsiCash::RefreshState(local_only, { "speed-boost" }));
        try { my_print(NOT_SENSITIVE, true, _T("%s: PsiCash state: %S"), __TFUNCTION__, PsiCash::GetDiagnosticInfo(true).dump(-1, ' ', true).c_str()); }
        catch (...) {}
    });

    if (!queued) {
        my_print(NOT_SENSITIVE, true, _T("%s: RefreshState not queued, as another is already queued"), __TFUNCTION__);
    }
}

void Lib::NewExpiringPurchase(
    const std::string& transactionClass,
    const std::string& distinguisher,
    const int64_t expectedPrice,
    std::function<void(error::Result<NewExpiringPurchaseResponse>)> callback)
{
    (void)m_requestQueue.dispatch((int)RequestType::NewExpiringPurchase, {}, [=] {
        callback(PsiCash::NewExpiringPurchase(transactionClass, distinguisher, expectedPrice));
        try { my_print(NOT_SENSITIVE, true, _T("%s: PsiCash state: %S"), __TFUNCTION__, PsiCash::GetDiagnosticInfo(true).dump(-1, ' ', true).c_str()); }
        catch (...) {}
    });
}

void Lib::AccountLogin(
    const std::string &utf8_username,
    const std::string &utf8_password,
    std::function<void(error::Result<AccountLoginResponse>)> callback)
{
    // Don't queue this if there is already an outstanding AccountLogin
    (void)m_requestQueue.dispatch(
        (int)RequestType::AccountLogin,
        {(int)RequestType::AccountLogin},
        [=] {
            callback(PsiCash::AccountLogin(utf8_username, utf8_password));
            try { my_print(NOT_SENSITIVE, true, _T("%s: PsiCash state: %S"), __TFUNCTION__, PsiCash::GetDiagnosticInfo(true).dump(-1, ' ', true).c_str()); }
            catch (...) {}
        });
}

void Lib::AccountLogout(
    std::function<void(error::Result<AccountLogoutResponse>)> callback)
{
    // Don't queue this if there is already an outstanding AccountLogout
    (void)m_requestQueue.dispatch(
        (int)RequestType::AccountLogout,
        {(int)RequestType::AccountLogout},
        [=] {
            callback(PsiCash::AccountLogout());
            try { my_print(NOT_SENSITIVE, true, _T("%s: PsiCash state: %S"), __TFUNCTION__, PsiCash::GetDiagnosticInfo(true).dump(-1, ' ', true).c_str()); }
            catch (...) {}
        });
}

// Note that this _requires_ a Psiphon tunnel to be in place.
psicash::MakeHTTPRequestFn GetHTTPReqFn(const StopInfo& stopInfo) {
    psicash::MakeHTTPRequestFn httpReqFn = [&stopInfo](const psicash::HTTPParams& params) ->psicash::HTTPResult {
        // NOTE: This makes only HTTPS requests and ignores params.scheme

        my_print(NOT_SENSITIVE, true, _T("%s: PsiCashLib starting request for: %hs"), __TFUNCTION__, params.path.c_str());

        wstringstream requestPath;
        requestPath << UTF8ToWString(params.path);
        if (!params.query.empty()) {
            requestPath << L"?";
        }
        for (size_t i = 0; i < params.query.size(); i++) {
            auto qp = params.query[i];
            if (i != 0) {
                requestPath << "&";
            }
            requestPath << UTF8ToWString(qp.first) << L"=" << UTF8ToWString(qp.second);
        }

        wstringstream headers;
        for (auto header : params.headers) {
            headers << UTF8ToWString(header.first) << L": " << UTF8ToWString(header.second) << L"\r\n";
        }

        HTTPResult result;

        HTTPSRequest httpsRequest(/*silentMode=*/true);
        HTTPSRequest::Response httpsResponse;

        try
        {
            if (!httpsRequest.MakeRequest(
                    UTF8ToWString(params.hostname).c_str(),
                    params.port,
                    "",         // webServerCertificate
                    requestPath.str().c_str(),
                    stopInfo,
                    HTTPSRequest::PsiphonProxy::REQUIRE,
                    httpsResponse,
                    true,       // failoverToURLProxy -- required for old WinXP
                    headers.str().empty() ? NULL : headers.str().c_str(),
                    params.body.empty() ? NULL : (LPVOID)params.body.c_str(),
                    params.body.length(),
                    UTF8ToWString(params.method).c_str()))
            {
                result.error = "httpsRequest.MakeRequest failed";
                return result;
            }
        }
        catch (StopSignal::StopException& ex)
        {
            // Application is disconnecting or exiting.
            my_print(NOT_SENSITIVE, true, _T("%s: PsiCashLib request interrupted by StopException: %d"), __TFUNCTION__, ex.GetType());
            result.code = HTTPResult::RECOVERABLE_ERROR;
            result.error = "request stopped by disconnection or exit";
            return std::move(result);
        }

        my_print(NOT_SENSITIVE, true, _T("%s: PsiCashLib completed request for: %hs; response code: %d"), __TFUNCTION__, params.path.c_str(), httpsResponse.code);

        result.code = httpsResponse.code;
        result.body = httpsResponse.body;
        result.headers = httpsResponse.headers;
        return std::move(result);
    };

    return httpReqFn;
}

} // namespace psicash
