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

#include "media/anime_list_utils.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void require(float actual, float expected, const char* message) {
  if (std::fabs(actual - expected) < 0.001f) return;
  std::cerr << message << ": got " << actual << ", expected " << expected << '\n';
  std::exit(EXIT_FAILURE);
}

}  // namespace

int main() {
  using anime::list::progressRatio;

  require(progressRatio(6, 12, 6), 0.5f, "a known count should give the watched share");
  require(progressRatio(15, 12, 15), 1.0f, "watching past the count should fill the bar");
  require(progressRatio(0, 12, 4), 0.0f, "nothing watched should leave the bar empty");

  // Estimated counts (12, 26, 52) come from `estimateEpisodeCount`
  require(progressRatio(3, 12, 3), 0.25f, "an estimated count should be used like a known one");
  require(progressRatio(0, 26, 14), 0.0f, "nothing watched should leave an estimated bar empty");

  // A long series with no estimate
  require(progressRatio(0, 0, 0), 0.0f, "nothing watched should leave the bar empty");
  require(progressRatio(60, 0, 60), 0.8f, "an up to date long series should stop at 80%");
  require(progressRatio(30, 0, 60), 0.4f, "a long series halfway through should be at 40%");
  require(progressRatio(70, 0, 60), 0.8f, "watching past the last aired should stop at 80%");

  return EXIT_SUCCESS;
}
