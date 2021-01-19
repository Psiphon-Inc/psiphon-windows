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

#include "subprocess.h"

class IPsiphonTunnelCoreNoticeHandler
{
public:
    /**
    Called for each Psiphon Tunnel Core notice read from the Psiphon Tunnel
    Core process when ConsumeSubprocessOutput is invoked on a PsiphonTunnelCore
    instance. See ConsumeSubprocessOutput in subprocess.h.
    */
    virtual void HandlePsiphonTunnelCoreNotice(const string& noticeType, const string& timestamp, const Json::Value& data) = 0;
};

/**
PsiphonTunnelCore is used to launch and manage a psiphon-tunnel-core
executable as a subprocess.
*/
class PsiphonTunnelCore : public Subprocess, ISubprocessOutputHandler
{
public:
    /**
    Initialize a new instance. Throws std::exception if outputHandler is null.
    */
    PsiphonTunnelCore(IPsiphonTunnelCoreNoticeHandler* noticeHandler, const tstring& exePath);
    ~PsiphonTunnelCore();

    // ISubprocessOutputHandler implementation
    void HandleSubprocessOutputLine(const string& line);

protected:
    IPsiphonTunnelCoreNoticeHandler *m_noticeHandler;
    bool m_panicked;
};

