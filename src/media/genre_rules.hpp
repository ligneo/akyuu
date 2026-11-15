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

#include <string>
#include <vector>

namespace anime {

// Each genre is either required, excluded or left alone. "Hide Ecchi" and "only Sports" cannot
// both be said with one set of checkboxes: the first hides a title for any genre it has, the
// second keeps it for any genre it has. AniList's own search uses the same three states.
struct GenreRules {
  std::vector<std::string> required;
  std::vector<std::string> excluded;

  bool empty() const;
};

// A title is left out when it has an excluded genre, or when some genres are required and it has
// none of them.
bool matchesGenreRules(const std::vector<std::string>& genres, const GenreRules& rules);

}  // namespace anime
