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

#include <QJsonArray>
#include <string>
#include <vector>

#include "track/feed_filter.hpp"

namespace track {

class FilterManager final {
public:
  const std::vector<FilterPreset>& presets() const;
  const std::vector<Filter>& filters() const;
  void setFilters(const std::vector<Filter>& filters);

  // Applies every filter to the feed, in the order v1 uses: regular filters first, then preference
  // filters, which need the rest of the feed to have been filtered already.
  void filter(Feed& feed) const;

  // Right-click actions on the torrents page, which work by adding or editing a filter.
  bool addDiscardFilter(const int animeId);
  std::vector<std::string> fansubFilter(const int animeId) const;
  bool setFansubFilter(const int animeId, const std::string& group,
                       const std::string& videoResolution);

  static QJsonArray toJson(const std::vector<Filter>& filters);
  static std::vector<Filter> fromJson(const QJsonArray& array);

private:
  void load() const;
  void save() const;

  mutable std::vector<Filter> filters_;
  mutable bool loaded_ = false;
};

inline FilterManager filterManager;

}  // namespace track
