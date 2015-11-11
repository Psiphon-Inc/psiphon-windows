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

#include "stdafx.h"
#include <WinSock2.h>
#include "logging.h"
#include "config.h"
#include "psiclient.h"
#include "utilities.h"
#include "diagnostic_info.h"
#include "server_list_reordering.h"


const int MAX_WORKER_THREADS = 30;
const int MAX_CHECK_TIME_MILLISECONDS = 5000;
const int RESPONSE_TIME_THRESHOLD_FACTOR = 2;

void ReorderServerList(ServerList& serverList, const StopInfo& stopInfo);


ServerListReorder::ServerListReorder()
    : m_thread(NULL), m_serverList(0)
{
    m_mutex = CreateMutex(NULL, FALSE, 0);
}


ServerListReorder::~ServerListReorder()
{
    // Ensure thread is not running.

    Stop(STOP_REASON_EXIT);
    CloseHandle(m_mutex);
}


void ServerListReorder::Start(ServerList* serverList)
{
    AutoMUTEX lock(m_mutex);

    m_serverList = serverList;

    if (m_stopSignal.CheckSignal(STOP_REASON_EXIT))
    {
        return;
    }

    Stop(STOP_REASON_CANCEL);

    if (!(m_thread = CreateThread(0, 0, ReorderServerListThread, this, 0, 0)))
    {
        my_print(NOT_SENSITIVE, false, _T("Server List Reorder: CreateThread failed (%d)"), GetLastError());
        return;
    }
}


void ServerListReorder::Stop(DWORD stopReason)
{
    AutoMUTEX lock(m_mutex);

    // This signal causes the thread to terminate
    m_stopSignal.SignalStop(stopReason);

    if (m_thread != NULL)
    {
        WaitForSingleObject(m_thread, INFINITE);

        // Reset for another run.

        m_thread = NULL;
    }

    m_stopSignal.ClearStopSignal(STOP_REASON_ALL &~ STOP_REASON_EXIT);
}


bool ServerListReorder::IsRunning()
{
    AutoMUTEX lock(m_mutex);

    return (m_thread != NULL);
}


DWORD WINAPI ServerListReorder::ReorderServerListThread(void* data)
{
    // No mutex here.  This is the main thread of execution that can be cancelled
    // by Stop().

    ServerListReorder* object = (ServerListReorder*)data;

    // Seed built-in non-crypto PRNG used for shuffling (load balancing)
    unsigned int seed = (unsigned)time(NULL);
    srand(seed);

    ReorderServerList(*(object->m_serverList), StopInfo(&object->m_stopSignal, STOP_REASON_ALL));

    object->m_thread = NULL;
    return 0;
}


struct WorkerThreadData
{
    ServerEntry m_entry;
    bool m_responded;
    unsigned int m_responseTime;
    StopInfo m_stopInfo;

    WorkerThreadData(ServerEntry entry, StopInfo stopInfo)
        : m_entry(entry),
          m_responded(false),
          m_responseTime(UINT_MAX),
          m_stopInfo(stopInfo)
    {
    }
};


DWORD WINAPI CheckServerReachabilityThread(void* object)
{
    WorkerThreadData* data = (WorkerThreadData*)object;

    // Test for reachability by establishing a TCP socket
    // connection to the specified port on the target host.

    DWORD start_time = GetTickCount();

    bool success = true;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    sockaddr_in serverAddr;
    WSAEVENT connectedEvent = NULL;
    WSANETWORKEVENTS networkEvents;
    SOCKET sock = INVALID_SOCKET;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(data->m_entry.serverAddress.c_str());
    // NOTE: we've already checked for the presence of a reachability port below
    serverAddr.sin_port = htons((unsigned short)data->m_entry.GetPreferredReachablityTestPort());

    connectedEvent = WSACreateEvent();

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (INVALID_SOCKET == sock ||
        0 != WSAEventSelect(sock, connectedEvent, FD_CONNECT) ||
        SOCKET_ERROR != connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) ||
        WSAEWOULDBLOCK != WSAGetLastError())
    {
        success = false;
    }

    while (success)
    {
        DWORD waitResult = WSAWaitForMultipleEvents(1, &connectedEvent, TRUE, 100, FALSE);

        if (WSA_WAIT_EVENT_0 == waitResult
            && 0 == WSAEnumNetworkEvents(sock, connectedEvent, &networkEvents)
            && (networkEvents.lNetworkEvents & FD_CONNECT)
            && networkEvents.iErrorCode[FD_CONNECT_BIT] == 0)
        {
            // Successfully connected
            break;
        }

        // Check for stop (abort) signal
        if (data->m_stopInfo.stopSignal->CheckSignal(data->m_stopInfo.stopReasons, false))
        {
            success = false;
            break;
        }
    }

    closesocket(sock);
    WSACloseEvent(connectedEvent);
    WSACleanup();

    DWORD end_time = GetTickCount(); // GetTickCount can wrap

    data->m_responseTime = (end_time >= start_time) ?
                           (end_time - start_time) :
                           (0xFFFFFFFF - start_time + end_time);

    data->m_responded = success;

    return 0;
}


void ReorderServerList(ServerList& serverList, const StopInfo& stopInfo)
{
    ServerEntries serverEntries = serverList.GetList();

    // Check response time from each server (in parallel).
    // At most the first MAX_WORKER_THREADS servers in the
    // current server list will be checked. We select the
    // first MAX/2 server from the top of the list (they
    // may be better/fresher) and then MAX/2 random servers
    // from the rest of the list (they may be underused).

    // TODO: use a thread pool?

    vector<HANDLE> threadHandles;
    vector<WorkerThreadData*> threadData;

    if (serverEntries.size() > MAX_WORKER_THREADS)
    {
        random_shuffle(serverEntries.begin() + MAX_WORKER_THREADS/2, serverEntries.end());
    }

    for (ServerEntryIterator entry = serverEntries.begin(); entry != serverEntries.end(); ++entry)
    {
        if (-1 != entry->GetPreferredReachablityTestPort())
        {
            WorkerThreadData* data = new WorkerThreadData(*entry, stopInfo);

            HANDLE threadHandle;
            if (!(threadHandle = CreateThread(0, 0, CheckServerReachabilityThread, (void*)data, 0, 0)))
            {
                continue;
            }

            threadHandles.push_back(threadHandle);
            threadData.push_back(data);

            if (threadHandles.size() >= MAX_WORKER_THREADS)
            {
                break;
            }
        }
    }

    // Wait for all threads to finish

    // TODO: stop waiting early if all threads finish?

    for (int waits = 0; waits < MAX_CHECK_TIME_MILLISECONDS/100; waits++)
    {
        Sleep(100);

        if (stopInfo.stopSignal->CheckSignal(stopInfo.stopReasons))
        {
            // Stop waiting early if exiting the app, etc.
            // NOTE: we still process results in this case
            break;
        }
    }
    stopInfo.stopSignal->SignalStop(STOP_REASON_CANCEL);

    for (vector<HANDLE>::iterator handle = threadHandles.begin(); handle != threadHandles.end(); ++handle)
    {
        WaitForSingleObject(*handle, INFINITE);
        CloseHandle(*handle);
    }

    // Build a list of all servers that responded within the threshold
    // time (+100%) of the best server. Using the best server as a base
    // is intended to factor out local network conditions, local cpu
    // conditions (e.g., SSL overhead) etc. We randomly shuffle the
    // resulting list for some client-side load balancing. Any server
    // that meets the threshold is considered equally qualified for
    // any position towards the top of the list.

    unsigned int fastestResponseTime = UINT_MAX;

    for (vector<WorkerThreadData*>::iterator data = threadData.begin(); data != threadData.end(); ++data)
    {
        my_print(
            SENSITIVE_LOG, 
            true,
            _T("server: %s, responded: %s, response time: %d"),
            UTF8ToWString((*data)->m_entry.serverAddress).c_str(),
            (*data)->m_responded ? L"yes" : L"no",
            (*data)->m_responseTime);

        if ((*data)->m_responded && (*data)->m_responseTime < fastestResponseTime)
        {
            fastestResponseTime = (*data)->m_responseTime;
        }

        Json::Value json;
        json["ipAddress"] = (*data)->m_entry.serverAddress;
        json["responded"] = (*data)->m_responded;
        json["responseTime"] = (*data)->m_responseTime;
        AddDiagnosticInfoJson("ServerResponseCheck", json);
    }

    ServerEntries respondingServers;

    for (vector<WorkerThreadData*>::iterator data = threadData.begin(); data != threadData.end(); ++data)
    {
        if ((*data)->m_responded && (*data)->m_responseTime <=
                fastestResponseTime*RESPONSE_TIME_THRESHOLD_FACTOR)
        {
            respondingServers.push_back((*data)->m_entry);
        }
    }

    random_shuffle(respondingServers.begin(), respondingServers.end());

    // Merge back into server entry list. MoveEntriesToFront will move
    // these servers to the top of the list in the order submitted. Any
    // other servers, including non-responders and new servers discovered
    // while this process ran will remain in position after the move-to-front
    // list. By using the ConnectionManager's ServerList object we ensure
    // there's no conflict while reading/writing the persistent server list.

    if (respondingServers.size() > 0)
    {
        serverList.MoveEntriesToFront(respondingServers);

        my_print(NOT_SENSITIVE, true, _T("Preferred servers: %d"), respondingServers.size());
    }

    // Cleanup

    for (vector<WorkerThreadData*>::iterator data = threadData.begin(); data != threadData.end(); ++data)
    {
        delete *data;
    }
}
