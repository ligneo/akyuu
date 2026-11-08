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

#include "track/availability.hpp"

#include <cstdlib>
#include <initializer_list>
#include <iostream>

#include "media/anime.hpp"

namespace {

using track::EpisodeRange;
using Ranges = std::vector<EpisodeRange>;

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

std::map<int, QString> onDisk(std::initializer_list<int> numbers) {
  std::map<int, QString> episodes;
  for (const int number : numbers) episodes.emplace(number, QString::number(number));
  return episodes;
}

std::map<int, QString> seriesWithout(const int episodes, const int absent) {
  std::map<int, QString> result;
  for (int number = 1; number <= episodes; ++number) {
    if (number != absent) result.emplace(number, QString::number(number));
  }
  return result;
}

void testFileEpisodes() {
  using track::fileEpisodes;

  require(fileEpisodes(EpisodeRange{5, 5}, 12) == EpisodeRange{5, 5}, "single episode");
  require(fileEpisodes(EpisodeRange{1, 2}, 12) == EpisodeRange{1, 2}, "file with two episodes");
  require(fileEpisodes(std::nullopt, 12) == EpisodeRange{1, 1}, "file without a number");
  require(fileEpisodes(EpisodeRange{12, 12}, 12) == EpisodeRange{12, 12}, "last episode");
  require(!fileEpisodes(EpisodeRange{13, 13}, 12), "beyond the episode count");
  require(!fileEpisodes(EpisodeRange{11, 13}, 12), "range ending beyond the episode count");
  require(fileEpisodes(EpisodeRange{300, 300}, 0) == EpisodeRange{300, 300}, "unknown count");
  require(!fileEpisodes(EpisodeRange{1, anime::kMaxEpisodeCount + 1}, 0), "absurd range");
}

void testEpisodeRanges() {
  using track::episodeRanges;

  require(episodeRanges({}).empty(), "no files");
  require(episodeRanges(onDisk({4})) == Ranges{{4, 4}}, "one file");
  require(episodeRanges(onDisk({1, 2, 3})) == Ranges{{1, 3}}, "consecutive files");
  require(episodeRanges(onDisk({1, 2, 4, 5, 9})) == Ranges{{1, 2}, {4, 5}, {9, 9}}, "gaps");
}

void testProgressBands() {
  using track::episodeRanges;
  using track::progressBands;

  // The case in issue 11: the gap belongs at episode 22, not at the end of the bar.
  require(progressBands(episodeRanges(seriesWithout(26, 22)), 8, 26) == Ranges{{8, 21}, {22, 26}},
          "missing episode keeps its place");

  require(progressBands(episodeRanges(seriesWithout(26, 26)), 8, 26) == Ranges{{8, 25}},
          "missing last episode");
  require(progressBands(Ranges{{9, 12}}, 8, 26) == Ranges{{8, 12}},
          "only episodes after the watched ones");
  require(progressBands(Ranges{{3, 4}, {11, 11}}, 1, 12) == Ranges{{2, 4}, {10, 11}},
          "episodes apart from each other");
  require(progressBands(Ranges{{1, 8}}, 8, 26).empty(), "everything on disk is watched");
  require(progressBands(Ranges{{1, 12}}, 8, 26) == Ranges{{8, 12}}, "cut at the watched part");
  require(progressBands(Ranges{{20, 30}}, 0, 24) == Ranges{{19, 24}}, "cut at the end of the bar");
  require(progressBands(Ranges{{0, 0}}, 0, 12).empty(), "episode zero has no place on the bar");
  require(progressBands({}, 3, 12).empty(), "no files");
}

}  // namespace

int main() {
  testFileEpisodes();
  testEpisodeRanges();
  testProgressBands();
  return EXIT_SUCCESS;
}
