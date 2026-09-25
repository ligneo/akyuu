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

#include "episode.hpp"

#include <QRegularExpression>
#include <algorithm>
#include <ranges>

#include "base/string.hpp"
#include "media/anime.hpp"

namespace track {

namespace {

// Anitomy may return an episode number with a suffix (e.g. `4a`) or a fraction (e.g. `7.5`). Only
// the leading digits are taken, as v1 did. Fractional numbers are moved into the anime title before
// identification, so they never reach this point.
int leadingNumber(const std::string& value) {
  static const QRegularExpression pattern{u"^\\d+"_s};
  const auto match = pattern.match(QString::fromStdString(value));
  return match.hasMatch() ? match.captured().toInt() : 0;
}

}  // namespace

Episode::Episode() : anime_id_{anime::kUnknownId} {}

auto Episode::find(const anitomy::ElementKind kind) const {
  const auto is_kind = [kind](const anitomy::Element& element) { return element.kind == kind; };
  return std::ranges::find_if(elements_, is_kind);
}

int Episode::animeId() const {
  return anime_id_;
}

void Episode::setAnimeId(int id) {
  anime_id_ = id;
}

std::optional<std::pair<int, int>> Episode::episodeNumberRange() const {
  const auto numbers = elements(anitomy::ElementKind::Episode);
  if (numbers.empty()) return std::nullopt;
  return std::pair{leadingNumber(numbers.front()), leadingNumber(numbers.back())};
}

void Episode::setEpisodeNumberRange(const std::pair<int, int>& range) {
  removeElements(anitomy::ElementKind::Episode);
  addElement(anitomy::ElementKind::Episode, std::to_string(range.first));
  if (range.second > range.first) {
    addElement(anitomy::ElementKind::Episode, std::to_string(range.second));
  }
}

const std::vector<anitomy::Element>& Episode::elements() const noexcept {
  return elements_;
}

void Episode::setElements(std::vector<anitomy::Element>& elements) {
  elements_ = elements;
}

bool Episode::contains(const anitomy::ElementKind kind) const {
  return find(kind) != elements_.end();
}

std::string Episode::element(const anitomy::ElementKind kind, const std::string placeholder) const {
  const auto it = find(kind);
  if (it != elements_.end()) return it->value;
  return placeholder;
};

std::vector<std::string> Episode::elements(const anitomy::ElementKind kind) const {
  const auto is_kind = [kind](const anitomy::Element& element) { return element.kind == kind; };
  const auto to_value = [](const anitomy::Element& element) { return element.value; };

  return elements_ | std::views::filter(is_kind) | std::views::transform(to_value) |
         std::ranges::to<std::vector>();
}

void Episode::addElement(const anitomy::ElementKind kind, const std::string& value) {
  elements_.emplace_back(anitomy::Element{.kind = kind, .value = value});
}

void Episode::removeElements(const anitomy::ElementKind kind) {
  const auto is_kind = [kind](const anitomy::Element& element) { return element.kind == kind; };
  std::erase_if(elements_, is_kind);
}

}  // namespace track
