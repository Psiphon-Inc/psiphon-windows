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

#include "feedback_upload_worker.h"


FeedbackUploadWorker::FeedbackUploadWorker(const string& diagnosticData, const bool vpnModeStarted,
                                           const string& upstreamProxyAddress, const StopInfo& stopInfo)
    : m_isVPNMode(vpnModeStarted)
{
    m_feedbackUpload = make_unique<FeedbackUpload>(diagnosticData, upstreamProxyAddress, stopInfo);
}


FeedbackUploadWorker::~FeedbackUploadWorker()
{
}


void FeedbackUploadWorker::StartUpload() const
{
    m_feedbackUpload->StartSendFeedback();
}


bool FeedbackUploadWorker::IsVPNMode() const
{
    return m_isVPNMode;
}


bool FeedbackUploadWorker::UploadCompleted() const
{
    return m_feedbackUpload->UploadCompleted();
}


bool FeedbackUploadWorker::UploadStopped() const
{
    return m_feedbackUpload->IsStopped();
}


bool FeedbackUploadWorker::UploadSuccessful() const
{
    return m_feedbackUpload->UploadStatus() == FEEDBACK_UPLOAD_STATUS_SUCCESS;
}
