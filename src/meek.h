/*
 * Copyright (c) 2014, Psiphon Inc.
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

class Meek : public IWorkerThread
{
public:
	Meek();
	virtual ~Meek(void);
	bool WaitForCmethodLine();
	int GetListenPort();

protected:
	bool DoStart();
	void DoStop(bool cleanly);
	void StopImminent();
	bool DoPeriodicCheck();
	void Cleanup();

private:
    bool StartMeek();
    bool CreateMeekPipe(HANDLE& o_outputPipe, HANDLE& o_errorPipe);
	bool ParseCmethodForPort(const char* buffer);

	tstring m_meekPath;
	tstring m_frontHostname;
	tstring m_meekServerURL; 
	int m_meekLocalPort;
	PROCESS_INFORMATION m_meekProcessInfo;
	HANDLE m_meekPipe;

};

