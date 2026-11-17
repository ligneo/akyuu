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
#include <string_view>
#include <vector>

#include "media/anime.hpp"

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

// Types follow the same three states as genres, so that "only movies" is one click rather than
// hiding every other type. A title has a single type: it is left out when its type is excluded,
// or when some types are required and its type is not one of them. A title of unknown type is
// never excluded, but it is not one of the required types either.
struct TypeRules {
  std::vector<Type> required;
  std::vector<Type> excluded;

  bool empty() const;
};

bool matchesTypeRules(Type type, const TypeRules& rules);

// The names kept in the settings file. The labels on the chips may change, these do not.
std::string_view typeSlug(Type type);
// Unknown names, duplicates and the unknown type itself are left out, so a hand-edited or damaged
// file cannot hide what it does not name.
std::vector<Type> typesFromSlugs(const std::vector<std::string>& slugs);
std::vector<std::string> slugsFromTypes(const std::vector<Type>& types);

}  // namespace anime
