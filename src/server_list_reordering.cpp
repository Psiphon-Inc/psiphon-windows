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
#include "config.h"
#include "httpsrequest.h"
#include "psiclient.h"
#include "server_list_reordering.h"


const int MAX_WORKER_THREADS = 50;


enum Result
{
    RESULT_DIDNT_TRY,
    RESULT_CERTIFICATE_ERROR,
    RESULT_SOCKET_ERROR,
    RESULT_HTTP_ERROR,
    RESULT_SUCCESS
};


struct WorkerThreadData
{
    ServerEntry m_entry;
    const bool& m_cancel;
    Result m_result;
    int m_responseTime;

    WorkerThreadData(ServerEntry entry, const bool& cancel)
        : m_entry(entry),
          m_cancel(cancel),
          m_result(RESULT_DIDNT_TRY),
          m_responseTime(0)
    {
    }
};


DWORD WINAPI CheckServerThread(void* object)
{
    WorkerThreadData* data = (WorkerThreadData*)object;

    DWORD start_time = GetTickCount();

    tstring requestPath =
        tstring(HTTP_CHECK_REQUEST_PATH) + 
        _T("?server_secret=") + NarrowToTString(data->m_entry.webServerSecret);

    HTTPSRequest httpsRequest;
    string response;
    bool requestSuccess = 
        httpsRequest.MakeRequest(
            data->m_cancel,
            NarrowToTString(data->m_entry.serverAddress).c_str(),
            data->m_entry.webServerPort,
            data->m_entry.webServerCertificate,
            requestPath.c_str(),
            response,
            false // use local proxy
            );

    DWORD end_time = GetTickCount(); // GetTickCount can wrap

    // TODO: get socket/cert/HTTP error info from WinHttpStatusCallback
    data->m_result = requestSuccess ? RESULT_SUCCESS : RESULT_SOCKET_ERROR;

    data->m_responseTime = (end_time >= start_time) ?
                           (end_time - start_time) :
                           (0xFFFFFFFF - start_time + end_time);

    return 0;
}

void ReorderServerList(
        ServerList& serverList,
        const bool& cancel)
{
    // TODO: only abort when app stops, not when tunnel is stopped
    // NOTE: when abort, will still process partial results

    ServerEntries serverEntries = serverList.GetList();

    // check each server (in parallel)
    // TODO: thread pool

    vector<HANDLE> threadHandles;
    vector<WorkerThreadData*> threadData;

    for (ServerEntryIterator entry = serverEntries.begin(); entry != serverEntries.end(); ++entry)
    {
        WorkerThreadData* data = new WorkerThreadData(*entry, cancel);

        HANDLE threadHandle;
        if (!(threadHandle = CreateThread(0, 0, CheckServerThread, (void*)data, 0, 0)))
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

    // wait for all threads to finish

    for (vector<HANDLE>::iterator handle = threadHandles.begin(); handle != threadHandles.end(); ++handle)
    {
        WaitForSingleObject(*handle, INFINITE);
    }

    for (vector<WorkerThreadData*>::iterator data = threadData.begin(); data != threadData.end(); ++data)
    {
        my_print(false, _T("server: %s, response time: %d"), NarrowToTString((*data)->m_entry.serverAddress).c_str(), (*data)->m_responseTime);
    }

    // sort into responding/non-responding buckets

    // TODO

    // sort responding bucket by time, then shuffle

    // TODO

    // merge back into server entry list (which may have been modified since we loaded it)

    // TODO

    // TODO: cleanup (deallocate, close thread handles)
}
