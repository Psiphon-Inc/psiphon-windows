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

#ifndef PSICASHLIB_DATETIME_H
#define PSICASHLIB_DATETIME_H

#include <chrono>
#include "vendor/nlohmann/json.hpp"

namespace psicash {
namespace datetime {

using Duration = std::chrono::milliseconds; // millisecond-resolution duration
using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<Clock, Duration>;

class DateTime {
public:
    // By default, initializes to the "zero" value.
    DateTime();
    DateTime(const DateTime& src);
    explicit DateTime(const TimePoint& src);
    DateTime& operator=(const DateTime&) = default;

    static DateTime Zero();

    static DateTime Now();

    // Returns true if this DateTime is the zero value.
    bool IsZero() const;

    std::string ToString() const;

    // These only support the "Z" timezone format.
    std::string ToISO8601() const;
    bool FromISO8601(const std::string& s);

    // Parses the HTTP Date header format
    bool FromRFC7231(const std::string&);

    Duration Diff(const DateTime& other) const;
    DateTime Add(const Duration& d) const;
    DateTime Sub(const Duration& d) const;

    // Mostly for testing
    int64_t MillisSinceEpoch() const;

    bool operator<(const DateTime& rhs) const;
    bool operator>(const DateTime& rhs) const;

    friend bool operator==(const DateTime& lhs, const DateTime& rhs);

    friend void to_json(nlohmann::json& j, const DateTime& dt);
    friend void from_json(const nlohmann::json& j, DateTime& dt);

private:
    TimePoint time_point_;
};


// These are intended to help de/serialization of duration values.
int64_t DurationToInt64(const Duration& d);
Duration DurationFromInt64(int64_t d);

} // namespace datetime
} // namespace psicash

#endif //PSICASHLIB_DATETIME_H
