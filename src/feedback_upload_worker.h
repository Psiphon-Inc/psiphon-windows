/*
 * Copyright(c) 2020, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software : you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "worker_thread.h"
#include "feedback_upload.h"

// FeedbackUploadWorker provides a convenience wrapper around the
// FeedbackUpload class which captures information relevant to the
// upload attempt in the context of the greater application, that
// is not relevant to the core logic of FeedbackUpload.
class FeedbackUploadWorker
{
public:
    FeedbackUploadWorker(const string& diagnosticData, const bool vpnModeStarted,
                         const string& upstreamProxyAddress, const StopInfo& stopInfo);
    ~FeedbackUploadWorker();

    /**
    Begins the feedback upload operation.
    May throw FeedbackUploadFailed, Error, or Abort.
    */
    void StartUpload() const;

    /**
    Returns the vpnModeStarted value that the FeedbackUploadWorker was initialized
    with.
    */
    bool IsVPNMode() const;

    /**
    Returns true if the feedback upload has either completed successfully or failed;
    otherwise returns false.
    */
    bool UploadCompleted() const;

    /**
    Returns true if the feedback upload has been stopped; otherwise returns false.
    */
    bool UploadStopped() const;

    /**
    Returns true if the feedback upload completed sucessfully; otherwise returns false.
    */
    bool UploadSuccessful() const;

protected:
    bool m_isVPNMode;
    unique_ptr<FeedbackUpload> m_feedbackUpload;
};
