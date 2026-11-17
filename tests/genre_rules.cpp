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

#include "media/genre_rules.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

}  // namespace

int main() {
  using anime::GenreRules;
  using anime::matchesGenreRules;

  const std::vector<std::string> actionEcchi{"Action", "Ecchi"};
  const std::vector<std::string> sportsDrama{"Sports", "Drama"};
  const std::vector<std::string> romance{"Romance"};
  const std::vector<std::string> none;

  require(GenreRules{}.empty(), "no rules should be empty");
  require(matchesGenreRules(actionEcchi, {}), "no rules should keep every title");
  require(matchesGenreRules(none, {}), "no rules should keep a title without genres");

  const GenreRules noEcchi{.excluded = {"Ecchi"}};
  require(!matchesGenreRules(actionEcchi, noEcchi), "an excluded genre should hide the title");
  require(matchesGenreRules(sportsDrama, noEcchi), "other titles should stay");
  require(matchesGenreRules(none, noEcchi), "a title without genres has nothing to exclude");

  const GenreRules onlySports{.required = {"Sports"}};
  require(matchesGenreRules(sportsDrama, onlySports), "one required genre should be enough");
  require(!matchesGenreRules(romance, onlySports), "a title without it should be hidden");
  require(!matchesGenreRules(none, onlySports), "a title without genres has none required");

  const GenreRules sportsOrRomance{.required = {"Sports", "Romance"}};
  require(matchesGenreRules(romance, sportsOrRomance), "any of the required genres should do");
  require(!matchesGenreRules(actionEcchi, sportsOrRomance), "none of them should hide the title");

  const GenreRules sportsNoDrama{.required = {"Sports"}, .excluded = {"Drama"}};
  require(!matchesGenreRules(sportsDrama, sportsNoDrama), "exclusion should win over requirement");
  require(matchesGenreRules(std::vector<std::string>{"Sports"}, sportsNoDrama),
          "a required genre without the excluded one should stay");

  using anime::Type;
  using anime::matchesTypes;
  using anime::slugsFromTypes;
  using anime::typesFromSlugs;

  require(matchesTypes(Type::Movie, {}), "no hidden types should keep every title");
  const std::vector<Type> noMovies{Type::Movie};
  require(!matchesTypes(Type::Movie, noMovies), "a hidden type should hide the title");
  require(matchesTypes(Type::Tv, noMovies), "other types should stay");
  require(matchesTypes(Type::Unknown, {Type::Unknown}), "an unknown type has no chip to hide it");
  std::vector<Type> allTypes{anime::kTypes.begin(), anime::kTypes.end()};
  require(matchesTypes(Type::Unknown, allTypes), "an unknown type should stay with all hidden");

  require(typesFromSlugs(slugsFromTypes(allTypes)) == allTypes,
          "every type should come back from its name");
  for (const auto type : anime::kTypes) {
    require(!anime::typeSlug(type).empty(), "every type with a chip needs a name");
  }
  require(typesFromSlugs({"", "TV", "unknown", "tv_short", "42", "movie", "movie"}) == noMovies,
          "damaged names should be left out and a name should count once");
  require(slugsFromTypes({Type::Unknown}).empty(), "the unknown type should not be saved");

  return EXIT_SUCCESS;
}
