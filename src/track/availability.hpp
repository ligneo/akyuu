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

#include <QString>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace track {

using EpisodeRange = std::pair<int, int>;

// The episodes one file holds, or nothing if the numbers cannot be right for the series.
std::optional<EpisodeRange> fileEpisodes(const std::optional<EpisodeRange>& parsed,
                                         const int episodeCount);

// Consecutive episode numbers joined into {first, last}.
std::vector<EpisodeRange> episodeRanges(const std::map<int, QString>& episodes);

// The parts of the progress bar to paint, as {from, to}. Episode n spans n - 1 to n.
std::vector<EpisodeRange> progressBands(const std::vector<EpisodeRange>& ranges, const int watched,
                                        const int episodes);

}  // namespace track
