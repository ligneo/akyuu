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

#include "genre_rules.hpp"

#include <algorithm>

namespace anime {

bool GenreRules::empty() const {
  return required.empty() && excluded.empty();
}

bool matchesGenreRules(const std::vector<std::string>& genres, const GenreRules& rules) {
  const auto has = [&genres](const std::string& genre) {
    return std::ranges::contains(genres, genre);
  };

  if (std::ranges::any_of(rules.excluded, has)) return false;
  if (!rules.required.empty() && !std::ranges::any_of(rules.required, has)) return false;

  return true;
}

}  // namespace anime
