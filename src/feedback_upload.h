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

#pragma once

#include "worker_thread.h"
#include "psiphon_tunnel_core.h"
#include "transport.h"
#include "transport_registry.h"
#include "usersettings.h"


// The feedback upload is in progress
#define FEEDBACK_UPLOAD_STATUS_IN_PROGRESS   0x0L

// The feedback upload succeeded
#define FEEDBACK_UPLOAD_STATUS_SUCCESS       (1L << 0)

// The feedback upload failed (the feedback upload subprocess exited with code 1)
#define FEEDBACK_UPLOAD_STATUS_FAILED        (1L << 1)

// The feedback upload was cancelled by the stop signal going high
#define FEEDBACK_UPLOAD_STATUS_CANCELLED     (1L << 2)

// An unexpected error occurred
#define FEEDBACK_UPLOAD_STATUS_ERROR         (1L << 3)

/**
Exception class

Indicates that the feedback upload was not successful.
*/
class FeedbackUploadFailed
{
    friend class FeedbackUpload;
public:
    FeedbackUploadFailed() {}
};

/**
FeedbackUpload facilitates uploading feedback data to Psiphon's servers. This
includes user feedback and diagnostic logs. The upload can, optionally, be
interrupted with the provided stop signal, when it is desirable to do so.
*/
class FeedbackUpload : public IWorkerThread, public IPsiphonTunnelCoreNoticeHandler
{

public:
    FeedbackUpload(const string& diagnosticData,
                   const string& upstreamProxyAddress,
                   const StopInfo& stopInfo);
    virtual ~FeedbackUpload();

    /**
    Begins the feedback upload operation. The status of the upload
    can be queried with FeedbackUpload::UploadStatus() or
    FeedbackUpload::UploadCompleted().
    May throw FeedbackUploadFailed, Error, or Abort.
    */
    void StartSendFeedback();

    /**
    Cleanup should be invoked to cleanup any underlying resources and will
    terminate the feedback upload subprocess if it is still running. Once
    called, this instance must only be used to check the final status of the
    feedback upload operation, and not for a new feedback upload. Subsequent
    upload attempts must be made with a new FeedbackUpload instance.
    Returns true if the cleanup succeeded, otherwise false.
    */
    virtual bool Cleanup();

    /**
    The upload has either completed successfully or failed.
    */
    bool FeedbackUpload::UploadCompleted() const;

    /**
    Feedback upload status. Possible values are defined by the constants
    FEEDBACK_UPLOAD_STATUS{IN_PROGRESS, SUCCESS, FAILED, CANCELLED, ERROR}.
    */
    DWORD FeedbackUpload::UploadStatus() const;

protected:
    // IWorkerThread implementation
    bool DoStart();
    void StopImminent();
    void DoStop(bool cleanly);
    virtual bool DoPeriodicCheck();

    // IPsiphonTunnelCoreNoticeHandler implementation
    void HandlePsiphonTunnelCoreNotice(const string& noticeType, const string& timestamp, const Json::Value& data);

    virtual void SendFeedback();

    /**
    May throw FeedbackUploadFailed.
    */
    void SendFeedbackHelper();
    bool SpawnFeedbackUploadProcess(const tstring& configFilename, const string& diagnosticData);

protected:
    tstring m_exePath;
    atomic<DWORD> m_uploadStatus;
    WorkerThreadSynch m_workerThreadSynch;
    string m_diagnosticData;
    string m_upstreamProxyAddress;
    StopInfo m_stopInfo;
    unique_ptr<PsiphonTunnelCore> m_psiphonTunnelCore;
};
