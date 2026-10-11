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

#include "track/episode_compare.hpp"

#include <cstdlib>
#include <iostream>

#include "media/anime.hpp"

namespace {

using anitomy::ElementKind;
using track::Episode;

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

Episode episode(const std::string& title, const std::string& number, int animeId) {
  Episode episode;
  episode.addElement(ElementKind::Title, title);
  episode.addElement(ElementKind::Episode, number);
  episode.setAnimeId(animeId);
  return episode;
}

}  // namespace

int main() {
  using track::hasEpisodeChanged;
  using track::isSameMedia;
  constexpr int unknown = anime::kUnknownId;

  const auto frieren5 = episode("Frieren", "5", 1);
  require(hasEpisodeChanged(std::nullopt, frieren5), "the first episode should be reported");
  require(!hasEpisodeChanged(frieren5, episode("Frieren", "5", 1)),
          "the same episode should not be reported again");
  require(hasEpisodeChanged(frieren5, episode("Frieren", "6", 1)),
          "the next episode should be reported");
  require(hasEpisodeChanged(frieren5, episode("Dandadan", "5", 2)),
          "another anime with the same number should be reported");

  // Two unrecognized titles with the same episode number are different episodes.
  const auto unknownA = episode("Unknown A", "3", unknown);
  const auto unknownB = episode("Unknown B", "3", unknown);
  require(hasEpisodeChanged(unknownA, unknownB), "another unknown title should be reported");
  require(!isSameMedia(unknownA, unknownB), "another unknown title should start a new session");

  // Identifying the anime by hand is reported, but keeps the session going.
  const auto identified = episode("Unknown A", "3", 7);
  require(hasEpisodeChanged(unknownA, identified), "identifying the anime should be reported");
  require(isSameMedia(unknownA, identified), "identifying the anime should keep the session");

  require(isSameMedia(frieren5, episode("Sousou no Frieren", "5", 1)),
          "another title of the same anime should keep the session");
  require(!isSameMedia(frieren5, episode("Frieren", "6", 1)),
          "the next episode should start a new session");
  require(!isSameMedia(frieren5, episode("Frieren", "5", unknown)),
          "losing the anime should start a new session");

  return EXIT_SUCCESS;
}
