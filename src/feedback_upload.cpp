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

#include "feedback_upload.h"
#include "authenticated_data_package.h"
#include "config.h"
#include "diagnostic_info.h"
#include "embeddedvalues.h"
#include "logging.h"
#include "psiclient.h"
#include "psiphon_tunnel_core_utilities.h"
#include "sessioninfo.h"
#include "systemproxysettings.h"
#include "utilities.h"

using namespace std::experimental;

#define EXE_NAME _T("feedback-upload.exe")

// IWorkerThread boilerplate

void FeedbackUpload::StartSendFeedback()
{
    if (!IWorkerThread::Start(m_stopInfo, &m_workerThreadSynch))
    {
        throw FeedbackUploadFailed();
    }
}

// Called once during IWorkerThread startup.
bool FeedbackUpload::DoStart()
{
    try
    {
        SendFeedback();
    }
    // NOTE: throws FeedbackUploadFailed, but this exception
    // currently carries no extra information with which to
    // act upon.
    catch (...)
    {
        m_uploadStatus = FEEDBACK_UPLOAD_STATUS_ERROR;
        return false;
    }

    return true;
}

// Called once when the IWorkerThread is about to be stopped.
void FeedbackUpload::StopImminent()
{
    // No pre-stop actions required.
}

// Called once when the IWorkerThread is stopped. Triggered by either the stop
// signal going high, or this IWorkerThread subclasses's DoPeriodicCheck
// implementation returning false.
void FeedbackUpload::DoStop(bool cleanly)
{
    if (cleanly) {
        // Stop was triggered by the stop signal and not by DoPeriodicCheck
        // returning false.
        m_uploadStatus = FEEDBACK_UPLOAD_STATUS_CANCELLED;
    }

    Cleanup();
}

/******************************************************************************
FeedbackUpload
******************************************************************************/

FeedbackUpload::FeedbackUpload(const string& diagnosticData,
                               const string& upstreamProxyAddress,
                               const StopInfo& stopInfo)
    : m_uploadStatus(FEEDBACK_UPLOAD_STATUS_IN_PROGRESS),
      m_diagnosticData(diagnosticData),
      m_stopInfo(stopInfo),
      m_upstreamProxyAddress(upstreamProxyAddress)
{
}


FeedbackUpload::~FeedbackUpload()
{
    (void)Cleanup();
    IWorkerThread::Stop();
}


bool FeedbackUpload::Cleanup()
{
    m_psiphonTunnelCore = nullptr;
    return true;
}


void FeedbackUpload::SendFeedback()
{
    my_print(NOT_SENSITIVE, true, _T("%s - starting feedback upload..."), __TFUNCTION__);

    try
    {
        SendFeedbackHelper();
    }
    catch (...)
    {
        (void)Cleanup();

        if (!m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons, false))
        {
            my_print(NOT_SENSITIVE, false, _T("%s - failed to start feedback upload"), __TFUNCTION__);
        }

        throw;
    }

    my_print(NOT_SENSITIVE, true, _T("%s - successfully started feedback upload"), __TFUNCTION__);
}


void FeedbackUpload::SendFeedbackHelper()
{
    WriteParameterFilesOut out;
    WriteParameterFilesIn in;
    in.requestingUrlProxyWithoutTunnel = false;
    // FeedbackUpload mode has a distinct config file so that it won't conflict
    // with a standard CoreTransport which may already be running.
    in.configFilename = WStringToUTF8(LOCAL_SETTINGS_APPDATA_FEEDBACK_CONFIG_FILENAME);
    in.upstreamProxyAddress = m_upstreamProxyAddress;
    in.encodedAuthorizations = Json::Value(Json::arrayValue);
    in.tempConnectServerEntry = NULL;

    if (!WriteParameterFiles(in, out))
    {
        throw FeedbackUploadFailed();
    }

    // Run subprocess; it will begin uploading the feedback
    if (!SpawnFeedbackUploadProcess(out.configFilePath, m_diagnosticData))
    {
        throw FeedbackUploadFailed();
    }

    return;
}


bool FeedbackUpload::SpawnFeedbackUploadProcess(const tstring& configFilename, const string& diagnosticData)
{
    tstringstream commandLineFlags;
    tstring exePath;

    if (!ExtractExecutable(
        IDR_PSIPHON_TUNNEL_CORE_EXE, EXE_NAME, exePath))
    {
        return false;
    }

    commandLineFlags << _T(" --config \"") << configFilename << _T("\" --feedbackUpload");

    m_psiphonTunnelCore = make_unique<PsiphonTunnelCore>(this, exePath);
    if (!m_psiphonTunnelCore->SpawnSubprocess(commandLineFlags.str())) {
        my_print(NOT_SENSITIVE, false, _T("%s:%d - SpawnSubprocess failed"), __TFUNCTION__, __LINE__);
        return false;
    }

    // Write diagnostics to stdin of the child process

    DWORD totalNumWritten = 0;
    while (totalNumWritten < diagnosticData.length()) {
        DWORD numWritten = 0;
        if (!WriteFile(m_psiphonTunnelCore->ParentInputPipe(), diagnosticData.c_str(), diagnosticData.length(), &numWritten, NULL)) {
            my_print(NOT_SENSITIVE, false, _T("%s - failed to write diagnostic data to subprocess stdin (%d)"), __TFUNCTION__, GetLastError());
            return false;
        }

        if (numWritten != diagnosticData.length()) {
            my_print(NOT_SENSITIVE, false, _T("%s - failed to write all diagnostic data to subprocess stdin, wrote %d of %d bytes"), __TFUNCTION__, numWritten, diagnosticData.length());
        }
        totalNumWritten += numWritten;
    }

    if (!m_psiphonTunnelCore->CloseInputPipes()) {
        my_print(NOT_SENSITIVE, false, _T("%s - failed to close input pipes"), __TFUNCTION__);
        return false;
    }

    return true;
}


bool FeedbackUpload::DoPeriodicCheck()
{
    // Notes:
    // - used in both IWorkerThread::Thread and in local TransportConnectHelper
    // - ConsumeCoreProcessOutput accesses local state (m_pipeBuffer) without
    //   a mutex. This is safe because one thread (IWorkerThread::Thread) is currently
    //   making all the calls to DoPeriodicCheck()

    // Check if the subprocess is still running, and consume any buffered output

    try {
        DWORD status = m_psiphonTunnelCore->Status();
        if (status == SUBPROCESS_STATUS_RUNNING) {
            if (m_stopInfo.stopSignal->CheckSignal(m_stopInfo.stopReasons))
            {
                throw Abort();
            }

            m_psiphonTunnelCore->ConsumeSubprocessOutput();

            return true;
        }
        else if (status == SUBPROCESS_STATUS_EXITED) {
            // The process has signalled -- which implies that it has died.
            // Consume any final output.
            m_psiphonTunnelCore->ConsumeSubprocessOutput();

            DWORD exitCode;
            if (!GetExitCodeProcess(m_psiphonTunnelCore->Process(), &exitCode)) {
                my_print(NOT_SENSITIVE, false, _T("%s - GetExitCodeProcess failed (%d)"), __TFUNCTION__, GetLastError());
                m_uploadStatus = FEEDBACK_UPLOAD_STATUS_ERROR;
                return false;
            }

            // If the exit code is 0, then the upload was successful.
            if (exitCode == 0) {
                m_uploadStatus = FEEDBACK_UPLOAD_STATUS_SUCCESS;
            }
            else if (exitCode == 1) {
                m_uploadStatus = FEEDBACK_UPLOAD_STATUS_FAILED;
            }
            else {
                my_print(NOT_SENSITIVE, false, _T("%s - unexpected exit code (%d)"), __TFUNCTION__, exitCode);
                m_uploadStatus = FEEDBACK_UPLOAD_STATUS_ERROR;
            }

            return false;
        }
        else if (status == SUBPROCESS_STATUS_NO_PROCESS) {
            return false;
        } else {
            my_print(NOT_SENSITIVE, false, _T("%s - unexpected subprocess status (%d)"), __TFUNCTION__, status);
            return false;
        }
    }
    catch (Subprocess::Error& error) {
        my_print(NOT_SENSITIVE, false, _T("%s - caught Subprocess::Error: %s"), __TFUNCTION__, error.GetMessage().c_str());
    }

    return false;
}


void FeedbackUpload::HandlePsiphonTunnelCoreNotice(const string& noticeType, const string& timestamp, const Json::Value& data)
{
}


bool FeedbackUpload::UploadCompleted() const
{
    return
        (m_uploadStatus == FEEDBACK_UPLOAD_STATUS_SUCCESS) ||
        (m_uploadStatus == FEEDBACK_UPLOAD_STATUS_FAILED) ||
        (m_uploadStatus == FEEDBACK_UPLOAD_STATUS_ERROR);
}

DWORD FeedbackUpload::UploadStatus() const
{
    return m_uploadStatus;
}
