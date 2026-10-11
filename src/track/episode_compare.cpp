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

#include "episode_compare.hpp"

#include "media/anime.hpp"

namespace track {

bool hasEpisodeChanged(const std::optional<Episode>& previous, const Episode& current) {
  if (!previous) return true;
  if (previous->animeId() != current.animeId()) return true;
  if (current.animeId() == anime::kUnknownId && previous->elements(anitomy::ElementKind::Title) !=
                                                    current.elements(anitomy::ElementKind::Title)) {
    return true;
  }

  return previous->elements(anitomy::ElementKind::Episode) !=
         current.elements(anitomy::ElementKind::Episode);
}

bool isSameMedia(const Episode& previous, const Episode& current) {
  if (previous.animeId() != anime::kUnknownId) {
    if (previous.animeId() != current.animeId()) return false;
  } else if (previous.elements(anitomy::ElementKind::Title) !=
             current.elements(anitomy::ElementKind::Title)) {
    return false;
  }

  return previous.elements(anitomy::ElementKind::Episode) ==
         current.elements(anitomy::ElementKind::Episode);
}

}  // namespace track
