/*
 * Copyright (c) 2018, Psiphon Inc.
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

#ifndef PSICASHLIB_ERROR_H
#define PSICASHLIB_ERROR_H

#include <string>
#include <vector>
#include <iosfwd>
#include "vendor/nonstd/expected.hpp"


namespace psicash {
namespace error {

/*
 * Error
 */

/// Represents an error value.
/// Boolean cast can be used to check if the Error is actually set.
/// If an error is "critical", then is results from something probably-unrecoverable, such
/// as a programming fault or an out-of-memory condition.
class Error {
public:
    Error();
    Error(const Error& src) = default;
    Error(bool critical, const std::string& message,
          const std::string& filename, const std::string& function, int line);
    Error& operator=(const Error&) = default;

    /// Wrapping a non-error results in a non-error (i.e., is a no-op). This allows it to be done
    /// unconditionally without introducing an error where there isn't one.
    /// Criticality isn't specified here -- only at the creation of the initial error.
    /// Returns *this.
    Error& Wrap(const std::string& message,
                const std::string& filename, const std::string& function, int line);

    Error& Wrap(const std::string& filename, const std::string& function, int line);

    bool Critical() const { return critical_; }

    /// Used to check if the current instance is an error or not.
    bool HasValue() const { return is_error_; }
    /// Used to check if the current instance is an error or not.
    operator bool() const { return HasValue(); }

    std::string ToString() const;
    friend std::ostream& operator<<(std::ostream& os, const Error& err);

private:
    // Indicates that this error is actually set. (There must be a more elegant way to do this...)
    bool is_error_;

    bool critical_;

    struct StackFrame {
        std::string message;
        std::string filename;
        std::string function;
        int line;
    };
    std::vector<StackFrame> stack_;
};

/// Used to represent a non-error Error.
// TODO: A more sophisticated implementation of this should model std::nullopt_t and nullopt.
const Error nullerr;

// Macros to assist with Error construction.
#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __func__
#endif
// Should be prefixed with namespaces: psicash::error::
#define MakeNoncriticalError(message)   Error(false, (message), __FILE__, __PRETTY_FUNCTION__, __LINE__)
#define MakeCriticalError(message)      Error(true, (message), __FILE__, __PRETTY_FUNCTION__, __LINE__)
#define WrapError(err, message)         ((err).Wrap((message), __FILE__, __PRETTY_FUNCTION__, __LINE__))
/// Like WrapError, but with no added message
#define PassError(err)                  ((err).Wrap(__FILE__, __PRETTY_FUNCTION__, __LINE__))


/*
 * Result
 */

/// Result holds an error-or-value. For usage, see nonstd::expected at
/// https://github.com/martinmoene/expected-lite/ or actual current usage.
/// To access the error (when `!res`), call `res.error()`.
template<typename T>
class Result : public nonstd::expected<T, Error> {
public:
    Result() = delete;

    Result(const T& val) : nonstd::expected<T, Error>(val) {}

    Result(const Error& err) : nonstd::expected<T, Error>(
            (nonstd::unexpected_type<Error>)err) {}
};

} // namespace error
} // namespace psicash

#endif //PSICASHLIB_ERROR_H
