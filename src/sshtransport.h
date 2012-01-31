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

#include "systemproxysettings.h"

class SessionInfo;
struct RegexReplace;


static const TCHAR* PLONK_SOCKS_PROXY_PORT = _T("1080");
static const TCHAR* POLIPO_HTTP_PROXY_PORT = _T("8080");
static int SSH_CONNECTION_TIMEOUT_SECONDS = 20;
static const TCHAR* PLONK_EXE_NAME = _T("psiphon3-plonk.exe");
static const TCHAR* POLIPO_EXE_NAME = _T("psiphon3-polipo.exe");


//
// Base class for the SSH transports
//
class SSHTransportBase: public ITransport
{
public:
    SSHTransportBase(ITransportManager* manager); 
    virtual ~SSHTransportBase();

    virtual tstring GetTransportName() const = 0;
    virtual tstring GetSessionID(SessionInfo sessionInfo) const;
    virtual tstring GetLastTransportError() const;

    virtual void WaitForDisconnect();
    virtual bool Cleanup();

protected:
    virtual void TransportConnect(const SessionInfo& sessionInfo);

    virtual bool GetSSHParams(
                    const SessionInfo& sessionInfo,
                    tstring& o_serverAddress, 
                    tstring& o_serverPort, 
                    tstring& o_serverHostKey, 
                    tstring& o_plonkCommandLine) = 0;

    void TransportConnectHelper(const SessionInfo& sessionInfo);
    void WaitForDisconnectHelper();
    bool ServerSSHCapable(const SessionInfo& sessionInfo);
    bool WaitForConnected();
    void Disconnect();
    void WaitAndDisconnect();
    void SignalDisconnect();

    bool CreatePolipoPipe(HANDLE& o_outputPipe, HANDLE& o_errorPipe);
    bool ProcessStatsAndStatus(bool connected);
    void UpsertPageView(const string& entry);
    void UpsertHttpsRequest(string entry);
    void ParsePolipoStatsBuffer(const char* page_view_buffer);

protected:
    SystemProxySettings m_systemProxySettings;
    tstring m_plonkPath;
    tstring m_polipoPath;
    PROCESS_INFORMATION m_plonkProcessInfo;
    PROCESS_INFORMATION m_polipoProcessInfo;
    HANDLE m_polipoPipe;
    DWORD m_lastStatusSendTimeMS;
    map<string, int> m_pageViewEntries;
    map<string, int> m_httpsRequestEntries;
    unsigned long long m_bytesTransferred;
    vector<RegexReplace> m_pageViewRegexes;
    vector<RegexReplace> m_httpsRequestRegexes;
};


//
// Standard SSH
//
class SSHTransport: public SSHTransportBase
{
public:
    SSHTransport(ITransportManager* manager); 
    virtual ~SSHTransport();

    virtual tstring GetTransportName() const;

protected:
    virtual bool GetSSHParams(
                    const SessionInfo& sessionInfo,
                    tstring& o_serverAddress, 
                    tstring& o_serverPort, 
                    tstring& o_serverHostKey, 
                    tstring& o_plonkCommandLine);
};


//
// Obfuscated SSH
//
class OSSHTransport: public SSHTransportBase
{
public:
    OSSHTransport(ITransportManager* manager); 
    virtual ~OSSHTransport();

    virtual tstring GetTransportName() const;

protected:
    virtual bool GetSSHParams(
                    const SessionInfo& sessionInfo,
                    tstring& o_serverAddress, 
                    tstring& o_serverPort, 
                    tstring& o_serverHostKey, 
                    tstring& o_plonkCommandLine);
};
