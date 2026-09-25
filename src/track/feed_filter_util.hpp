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

#include <QString>
#include <optional>
#include <vector>

#include "track/feed_filter.hpp"

namespace track::util {

// Filters travel between users as one line of text: v1's XML, deflated and Base64'd behind a "TAI"
// header. The format is kept byte for byte so that strings shared by v1 users still work.
QString encodeFilters(const std::vector<Filter>& filters);
std::optional<std::vector<Filter>> decodeFilters(const QString& input);

// The XML inside that string, which is also what v1 writes.
QString filtersToXml(const std::vector<Filter>& filters);
std::optional<std::vector<Filter>> filtersFromXml(const QString& xml);

// Filters are stored by shortcode rather than by index, so that reordering an enumeration does not
// change what is already on disk. The vocabulary is v1's, which also makes its filters readable.
QString shortcode(const FilterAction value);
QString shortcode(const FilterElement value);
QString shortcode(const FilterMatch value);
QString shortcode(const FilterOperator value);
QString shortcode(const FilterOption value);

std::optional<FilterAction> filterAction(const QString& shortcode);
std::optional<FilterElement> filterElement(const QString& shortcode);
std::optional<FilterMatch> filterMatch(const QString& shortcode);
std::optional<FilterOperator> filterOperator(const QString& shortcode);
std::optional<FilterOption> filterOption(const QString& shortcode);

}  // namespace track::util
