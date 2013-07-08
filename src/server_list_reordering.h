/*
 * Copyright (c) 2012, Psiphon Inc.
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

#include "serverlist.h"
#include "stopsignal.h"


class ServerListReorder
{
public:
    ServerListReorder();
    virtual ~ServerListReorder();

    void Start(ServerList* serverList);
    void Stop(DWORD stopReason);
    bool IsRunning();

private:
    static DWORD WINAPI ReorderServerListThread(void* data);

    HANDLE m_mutex;
    HANDLE m_thread;
    ServerList* m_serverList;

    // We use a custom stop signal because we only want to respond to Stop()
    // being called, and no other events.
    StopSignal m_stopSignal;
};
