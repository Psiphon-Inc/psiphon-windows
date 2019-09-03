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

#ifndef PSICASHLIB_URL_H
#define PSICASHLIB_URL_H

#include "error.hpp"

namespace psicash {

class URL {
public:
    error::Error Parse(const std::string& s);
    std::string ToString() const;

    /// URL encodes the given string.
    /// If `full` is true, the whole string will be percent-hex encoded, rather
    /// than allowing some characters through unchanged.
    static std::string Encode(const std::string& s, bool full);

public:
    std::string scheme_host_path_;
    std::string query_;
    std::string fragment_;
};

} // namespace psicash

#endif //PSICASHLIB_URL_H
