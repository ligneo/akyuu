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

#include "anime_stats.hpp"

#include <cmath>

#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "media/anime_utils.hpp"

namespace anime {

namespace {

std::chrono::seconds episodeDuration(const Details& item) {
  return std::chrono::minutes{estimateEpisodeLength(item)};
}

int watchedEpisodes(const Details& item, const list::Entry& entry) {
  return entry.watched_episodes + (entry.rewatched_times * item.episode_count);
}

}  // namespace

Statistics computeStatistics() {
  Statistics stats;

  std::vector<int> scores;

  for (const auto& entry : db.entries()) {
    const auto item = db.item(entry.anime_id);
    if (!item) continue;

    ++stats.anime_count;

    const auto episodes = watchedEpisodes(*item, entry);
    stats.episode_count += episodes;
    stats.time_spent_watching += episodeDuration(*item) * episodes;

    switch (entry.status) {
      case list::Status::Completed:
      case list::Status::Dropped:
        break;
      default: {
        const auto remaining = estimateEpisodeCount(*item, estimateLastAiredEpisodeNumber(*item)) -
                               entry.watched_episodes;
        if (remaining > 0) {
          stats.time_to_complete += episodeDuration(*item) * remaining;
        }
        break;
      }
    }

    if (entry.score > 0) {
      scores.push_back(entry.score);
      const auto bucket = static_cast<size_t>(entry.score / 10);
      if (bucket < kScoreBucketCount) ++stats.score_count[bucket];
    }
  }

  if (!scores.empty()) {
    float sum = 0.0f;
    for (const auto score : scores) sum += static_cast<float>(score);
    stats.score_mean = sum / scores.size();

    float sum_squares = 0.0f;
    for (const auto score : scores) {
      sum_squares += std::pow(static_cast<float>(score) - stats.score_mean, 2.0f);
    }
    stats.score_deviation = std::sqrt(sum_squares / scores.size());
  }

  return stats;
}

}  // namespace anime
