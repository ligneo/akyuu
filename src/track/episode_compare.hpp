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

#include <optional>

#include "track/episode.hpp"

namespace track {

// Tells detection to report the episode again. Identifying an unknown anime counts as a change, so
// that what is shown follows it.
bool hasEpisodeChanged(const std::optional<Episode>& previous, const Episode& current);

// Tells an update session to keep counting. Identifying an unknown anime does not make it a new
// one, but another title with the same episode number does.
bool isSameMedia(const Episode& previous, const Episode& current);

}  // namespace track
