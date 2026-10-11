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

#include "media_candidates.hpp"

#include <algorithm>
#include <tuple>

namespace track::media {

namespace {

bool isSession(const anisthesia::Media& media) {
  return media.state != anisthesia::MediaState::Unknown || media.information.size() > 1;
}

bool isSameMedia(const anisthesia::Media& a, const anisthesia::Media& b) {
  return std::ranges::equal(a.information, b.information, [](const auto& x, const auto& y) {
    return x.type == y.type && x.value == y.value;
  });
}

}  // namespace

std::vector<anisthesia::Media> splitMedia(const anisthesia::Result& result) {
  std::vector<anisthesia::Media> candidates;
  std::optional<size_t> merged;

  for (const auto& media : result.media) {
    if (isSession(media)) {
      candidates.push_back(media);
      continue;
    }
    if (!merged) {
      merged = candidates.size();
      candidates.emplace_back();
    }
    candidates[*merged].information.append_range(media.information);
  }

  return candidates;
}

bool isPaused(const anisthesia::Media& media) {
  return media.state == anisthesia::MediaState::Paused;
}

std::vector<Candidate> orderCandidates(const std::vector<anisthesia::Result>& results,
                                       const anisthesia::PlayerId& currentPlayerId,
                                       const std::optional<anisthesia::Media>& currentMedia) {
  std::vector<Candidate> candidates;

  for (const auto& result : results) {
    for (auto& media : splitMedia(result)) {
      candidates.push_back({&result, std::move(media)});
    }
  }

  // A paused session comes last, so that a tab left open does not hide an episode playing
  // elsewhere. Otherwise the session followed so far comes first, so that another one does not
  // take over while it is still playing.
  std::ranges::stable_sort(candidates, {}, [&](const Candidate& candidate) {
    const bool samePlayer = candidate.result->id == currentPlayerId;
    const bool sameMedia =
        samePlayer && currentMedia && isSameMedia(candidate.media, *currentMedia);
    return std::tuple{isPaused(candidate.media), !samePlayer, !sameMedia};
  });

  return candidates;
}

}  // namespace track::media
