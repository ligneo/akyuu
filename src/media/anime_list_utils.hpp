/**
 * Akyuu
 * Copyright (C) 2010-2026, Eren Okka
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

#include <algorithm>

namespace anime {
struct Details;
}

namespace anime::list {

struct Entry;

float getProgressRatio(const Details* item, const Entry* entry);

// `total` is the episode count, known or estimated, and 0 for a long series without an end in
// sight. Such a series is drawn up to 80%, as in v1, so the bar never looks finished.
constexpr float progressRatio(const int watched, const int total, const int lastAired) {
  if (watched <= 0) return 0.0f;
  if (total > 0) return std::min(watched / static_cast<float>(total), 1.0f);
  return std::min(0.8f, 0.8f * watched / std::max(lastAired, watched));
}

bool isInList(const Entry* entry);

Entry entryWithEpisodeWatched(const Details& item, const Entry* entry, const int number);

void save(Entry entry);
void remove(const int animeId);

}  // namespace anime::list
