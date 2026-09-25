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
#include <string>
#include <vector>

#include "track/feed.hpp"

namespace track {

// The order is the one v1 uses when listing elements, so that the condition dialog reads the same.
enum class FilterElement {
  MetaId,
  MetaStatus,
  MetaType,
  MetaEpisodes,
  MetaDateStart,
  MetaDateEnd,
  UserNotes,
  UserStatus,
  LocalEpisodeAvailable,
  EpisodeTitle,
  EpisodeNumber,
  EpisodeVersion,
  EpisodeGroup,
  EpisodeVideoResolution,
  EpisodeVideoType,
  FileTitle,
  FileCategory,
  FileDescription,
  FileLink,
  FileSize,
};

enum class FilterOperator {
  Equals,
  NotEquals,
  IsGreaterThan,
  IsGreaterThanOrEqualTo,
  IsLessThan,
  IsLessThanOrEqualTo,
  BeginsWith,
  EndsWith,
  Contains,
  NotContains,
};

enum class FilterMatch {
  All,
  Any,
};

enum class FilterAction {
  Discard,
  Select,
  Prefer,
};

// Determines how a discarded item is presented, which is why each value maps onto one of the
// discarded item states.
enum class FilterOption {
  Default,
  Deactivate,
  Hide,
};

struct FilterCondition {
  FilterElement element = FilterElement::FileTitle;
  FilterOperator op = FilterOperator::Equals;
  std::string value;
};

struct Filter {
  std::string name;
  bool enabled = true;
  FilterAction action = FilterAction::Discard;
  FilterMatch match = FilterMatch::All;
  FilterOption option = FilterOption::Default;
  std::vector<int> anime_ids;
  std::vector<FilterCondition> conditions;
};

struct FilterPreset {
  std::string description;
  Filter filter;
  bool is_default = false;
};

// Returns true when the filter had an effect on the item. `recursive` is false while a preference
// filter is comparing the item it matched against the rest of the feed.
bool applyFilter(const Filter& filter, Feed& feed, FeedItem& item, const bool recursive);
bool applyPreferenceFilter(const Filter& filter, Feed& feed, FeedItem& item);

}  // namespace track
