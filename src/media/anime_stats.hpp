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

#include <array>
#include <chrono>

namespace anime {

// Scores are stored in the 0-100 range, and grouped in buckets of ten.
inline constexpr size_t kScoreBucketCount = 11;

struct Statistics {
  int anime_count = 0;
  int episode_count = 0;
  std::chrono::seconds time_spent_watching{0};
  std::chrono::seconds time_to_complete{0};
  float score_mean = 0.0f;
  float score_deviation = 0.0f;
  std::array<int, kScoreBucketCount> score_count{};
};

Statistics computeStatistics();

}  // namespace anime
