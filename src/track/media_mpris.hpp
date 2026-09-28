/**
 * Akyuu
 * Copyright (C) 2026, cenky <cenkkgl@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <anisthesia.hpp>
#include <vector>

namespace track::media {

// Web browsers do not expose their tabs to other processes on Linux, but they report playing media
// via MPRIS. Results are returned in the same form as Anisthesia's, so they can be handled alike.
std::vector<anisthesia::Result> getMprisResults(const std::vector<anisthesia::Player>& players);

}  // namespace track::media
