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

//
// Stop conditions
//

// A combination of each stop reason that should result in stopping an established,
// or establishing, tunnel.
// Not to be used as an actual signal.
#define STOP_REASON_ANY_STOP_TUNNEL STOP_REASON_USER_DISCONNECT | STOP_REASON_EXIT | \
                                    STOP_REASON_UNEXPECTED_DISCONNECT | STOP_REASON_CANCEL

// A combination of each stop reason that should result in aborting a feedback
// upload operation when VPN mode has been started. Interrupts the upload when
// the connection state changes because this will disrupt the upload since all
// system traffic is tunneled in VPN mode.
// Not to be used as an actual signal.
#define STOP_REASON_ANY_ABORT_FEEDBACK_UPLOAD_VPN_MODE_STARTED STOP_REASON_CONNECTING | STOP_REASON_DISCONNECTING | \
                                                               STOP_REASON_EXIT | STOP_REASON_UNEXPECTED_DISCONNECT | \
                                                               STOP_REASON_CANCEL

// A combination of each stop reason that should result in aborting a feedback
// upload operation when the transport is connected and that transport is not the
// VPN transport. The feedback upload will be proxied through the local proxy
// exposed by the transport when it is connected; so the upload should be cancelled
// when the transport is no longer connected because the proxy will become unavailable
// and the ongoing proxied upload will eventually fail.
// Not to be used as an actual signal.
#define STOP_REASON_ANY_ABORT_FEEDBACK_UPLOAD_NONVPN_MODE_CONNECTED STOP_REASON_USER_DISCONNECT | STOP_REASON_CONNECTING | \
                                                                    STOP_REASON_DISCONNECTING | STOP_REASON_DISCONNECTED | \
                                                                    STOP_REASON_EXIT | STOP_REASON_UNEXPECTED_DISCONNECT | \
                                                                    STOP_REASON_CANCEL

// A combination of each stop reason that should result in aborting a feedback
// upload operation when either: a transport is started, but not connected, and that
// transport is not the VPN transport; or the transport is disconnected. Interrupts
// the upload when the transport has connected successfully because the upload should
// be restarted and proxied through the local proxy exposed by the transport at this
// point.
// Not to be used as an actual signal.
#define STOP_REASON_ANY_ABORT_FEEDBACK_UPLOAD_NONVPN_MODE_NOT_CONNECTED STOP_REASON_CANCEL | STOP_REASON_EXIT | STOP_REASON_CONNECTED

// Indicates no reasons should be matched (in a bitwise-AND check).
// Not to be used an actual signal.
#define STOP_REASON_NONE                      0x0L

// The user clicked the disconnect button
#define STOP_REASON_USER_DISCONNECT           (1L << 0)

// The transport is connecting (user initiated)
#define STOP_REASON_CONNECTING                (1L << 1)

// The transport is connected
#define STOP_REASON_CONNECTED                 (1L << 2)

// The transport is disconnecting (user initiated)
#define STOP_REASON_DISCONNECTING             (1L << 3)

// The transport is disconnected
#define STOP_REASON_DISCONNECTED              (1L << 4)

// The application is exiting (probably user-initiated, but maybe system-shutdown-initiated)
#define STOP_REASON_EXIT                      (1L << 5)

// The connected transport unexpectedly disconnected
#define STOP_REASON_UNEXPECTED_DISCONNECT     (1L << 6)

// A module can use this to cancel itself or its threads
#define STOP_REASON_CANCEL                    (1L << 7)

// Base class that can be used for implementing stop signals.
// Can also be used directly if no customizations are necessary.
class StopSignal
{
public:
    // Base class for exceptions
    class StopException : public std::exception
    {
    public:
        virtual DWORD GetType() const = 0;
    };

    // will never be thrown
    class NoStopException : public StopException
    {
    public:
        virtual DWORD GetType() const { return STOP_REASON_NONE; }
    };

    // TODO: Should these subclasses be in the subclasses of StopSignal?

    class UserDisconnectException : public StopException
    {
    public:
        virtual DWORD GetType() const { return STOP_REASON_USER_DISCONNECT; }
    };

    class ExitStopException : public StopException
    {
    public:
        virtual DWORD GetType() const { return STOP_REASON_EXIT; }
    };

    class UnexpectedDisconnectStopException : public StopException
    {
    public:
        virtual DWORD GetType() const { return STOP_REASON_UNEXPECTED_DISCONNECT; }
    };

    // Check if the current state of the stop signal matches any of the
    // bitwise-OR'd `reasons`. Returns the matching reasons, or 0 if no match.
    // If `throwIfTrue` is true, an exception is thrown.
    virtual DWORD CheckSignal(DWORD reasons, bool throwIfTrue=false) const;

    // Sets the stop signal to the given reason. (More specifically, ORs
    // the stop signal into the currently set reasons.)
    virtual void SignalStop(DWORD reason);

    // Removes `reason` from the set of currently set reasons.
    virtual void ClearStopSignal(DWORD reason);

    static void ThrowSignalException(DWORD reason);

    StopSignal();
    virtual ~StopSignal();

private:
    HANDLE m_mutex;
    DWORD m_stop;
};

// Convenience struct for passing around a stop signal and set of reasons
struct StopInfo
{
    StopSignal* stopSignal;
    DWORD stopReasons;
    StopInfo() : stopSignal(NULL), stopReasons(STOP_REASON_NONE) {}
    StopInfo(StopSignal* stopSignal, DWORD stopReasons) : stopSignal(stopSignal), stopReasons(stopReasons) {}
};

//
// Singleton class providing access to the global stop conditions
//
class GlobalStopSignal : public StopSignal
{
public:
    // returns the instance of this singleton
    static GlobalStopSignal& Instance();

private:
    static void CleanUp();

    GlobalStopSignal();
    ~GlobalStopSignal();

    // not copyable
    GlobalStopSignal(GlobalStopSignal const&);
    GlobalStopSignal& operator=(GlobalStopSignal const&);

    static GlobalStopSignal* MInstance;
};
