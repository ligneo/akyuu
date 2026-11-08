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

#include "availability.hpp"

#include <algorithm>

#include "media/anime.hpp"

namespace track {

std::optional<EpisodeRange> fileEpisodes(const std::optional<EpisodeRange>& parsed,
                                         const int episodeCount) {
  // A file without an episode number is taken to be the first episode, as in v1.
  const auto range = parsed.value_or(EpisodeRange{1, 1});

  // Anything beyond the known episode count is a misparse rather than a new episode.
  const int limit = episodeCount > 0 ? episodeCount : anime::kMaxEpisodeCount;
  if (range.second > limit) return std::nullopt;

  return range;
}

std::vector<EpisodeRange> episodeRanges(const std::map<int, QString>& episodes) {
  std::vector<EpisodeRange> ranges;

  for (const auto& [number, path] : episodes) {
    if (!ranges.empty() && ranges.back().second == number - 1) {
      ranges.back().second = number;
    } else {
      ranges.emplace_back(number, number);
    }
  }

  return ranges;
}

std::vector<EpisodeRange> progressBands(const std::vector<EpisodeRange>& ranges, const int watched,
                                        const int episodes) {
  std::vector<EpisodeRange> bands;

  for (const auto& [first, last] : ranges) {
    const int from = std::max(first - 1, watched);
    const int to = std::min(last, episodes);
    if (to > from) bands.emplace_back(from, to);
  }

  return bands;
}

}  // namespace track
