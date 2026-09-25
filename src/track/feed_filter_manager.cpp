/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
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

#include "feed_filter_manager.hpp"

#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include "akyuu/settings.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "media/anime_utils.hpp"
#include "track/feed_filter_util.hpp"

namespace track {

namespace {

using namespace Qt::StringLiterals;

std::string numberValue(const int value) {
  return std::to_string(value);
}

// v1's preset list, in the same order. The ones marked as default make up a new user's filters.
std::vector<FilterPreset> buildPresets() {
  std::vector<FilterPreset> presets;

  const auto add = [&presets](const FilterAction action, const FilterMatch match,
                              const bool isDefault, const FilterOption option, const char* name,
                              const char* description) {
    FilterPreset preset;
    preset.description = QCoreApplication::translate("track", description).toStdString();
    preset.is_default = isDefault;
    preset.filter.name = QCoreApplication::translate("track", name).toStdString();
    preset.filter.enabled = true;
    preset.filter.action = action;
    preset.filter.match = match;
    preset.filter.option = option;
    presets.push_back(std::move(preset));
  };

  const auto addCondition = [&presets](const FilterElement element, const FilterOperator op,
                                       const std::string& value) {
    presets.back().filter.conditions.push_back({element, op, value});
  };

  // Preset filters

  add(FilterAction::Discard, FilterMatch::All, false, FilterOption::Default, "(Custom)",
      "Lets you create a custom filter from scratch");

  add(FilterAction::Prefer, FilterMatch::All, false, FilterOption::Default, "[Fansub] Anime",
      "Lets you choose a fansub group for one or more anime");
  addCondition(FilterElement::EpisodeGroup, FilterOperator::Equals, "TaigaSubs (change this)");

  add(FilterAction::Discard, FilterMatch::Any, false, FilterOption::Default,
      "Discard bad video keywords",
      "Discards everything that is AVI, DIVX, LQ, RMVB, SD, WMV or XVID");
  for (const auto keyword : {"AVI", "DIVX", "LQ", "RMVB", "SD", "WMV", "XVID"}) {
    addCondition(FilterElement::EpisodeVideoType, FilterOperator::Contains, keyword);
  }

  add(FilterAction::Prefer, FilterMatch::Any, false, FilterOption::Default, "Prefer new versions",
      "Prefers v2 files and above when there are earlier releases of the same episode as well");
  addCondition(FilterElement::EpisodeVersion, FilterOperator::IsGreaterThan, "1");

  // Default filters

  add(FilterAction::Select, FilterMatch::Any, true, FilterOption::Default,
      "Select currently watching",
      "Selects files that belong to anime that you're currently watching");
  addCondition(FilterElement::UserStatus, FilterOperator::Equals,
               numberValue(static_cast<int>(anime::list::Status::Watching)));

  add(FilterAction::Select, FilterMatch::All, true, FilterOption::Default,
      "Select airing anime in plan to watch",
      "Selects files that belong to an airing anime that you're planning to watch");
  addCondition(FilterElement::MetaStatus, FilterOperator::Equals,
               numberValue(static_cast<int>(anime::Status::Airing)));
  addCondition(FilterElement::UserStatus, FilterOperator::Equals,
               numberValue(static_cast<int>(anime::list::Status::PlanToWatch)));

  add(FilterAction::Discard, FilterMatch::All, true, FilterOption::Default, "Discard dropped",
      "Discards files that belong to anime that you've dropped watching");
  addCondition(FilterElement::UserStatus, FilterOperator::Equals,
               numberValue(static_cast<int>(anime::list::Status::Dropped)));

  add(FilterAction::Discard, FilterMatch::Any, true, FilterOption::Deactivate,
      "Discard and deactivate not-in-list anime",
      "Discards files that do not belong to any anime in your list");
  addCondition(FilterElement::UserStatus, FilterOperator::Equals,
               numberValue(static_cast<int>(anime::list::Status::NotInList)));

  add(FilterAction::Discard, FilterMatch::Any, true, FilterOption::Default,
      "Discard watched and available episodes",
      "Discards episodes you've already watched or downloaded");
  addCondition(FilterElement::EpisodeNumber, FilterOperator::IsLessThanOrEqualTo, "%watched%");
  addCondition(FilterElement::LocalEpisodeAvailable, FilterOperator::Equals, "True");

  add(FilterAction::Prefer, FilterMatch::Any, true, FilterOption::Default,
      "Prefer high-resolution files",
      "Prefers 1080p files when there are other files of the same episode as well");
  addCondition(FilterElement::EpisodeVideoResolution, FilterOperator::Equals, "1080p");

  return presets;
}

}  // namespace

const std::vector<FilterPreset>& FilterManager::presets() const {
  // Built on demand, because the descriptions are translated.
  static const auto presets = buildPresets();
  return presets;
}

const std::vector<Filter>& FilterManager::filters() const {
  if (!loaded_) load();
  return filters_;
}

void FilterManager::setFilters(const std::vector<Filter>& filters) {
  filters_ = filters;
  loaded_ = true;
  save();
}

void FilterManager::load() const {
  loaded_ = true;

  const auto array = akyuu::settings.torrentFilters();

  if (!array) {
    // Nothing has been set up yet, so the user starts with the default presets, as in v1. An
    // empty list, on the other hand, means every filter was removed on purpose.
    for (const auto& preset : presets()) {
      if (preset.is_default) filters_.push_back(preset.filter);
    }
    return;
  }

  filters_ = fromJson(*array);
}

void FilterManager::save() const {
  akyuu::settings.setTorrentFilters(toJson(filters_));
}

QJsonArray FilterManager::toJson(const std::vector<Filter>& filters) {
  QJsonArray array;

  for (const auto& filter : filters) {
    QJsonArray animeIds;
    for (const auto id : filter.anime_ids) animeIds.append(id);

    QJsonArray conditions;
    for (const auto& condition : filter.conditions) {
      conditions.append(QJsonObject{
          {u"element"_s, util::shortcode(condition.element)},
          {u"operator"_s, util::shortcode(condition.op)},
          {u"value"_s, QString::fromStdString(condition.value)},
      });
    }

    array.append(QJsonObject{
        {u"name"_s, QString::fromStdString(filter.name)},
        {u"enabled"_s, filter.enabled},
        {u"action"_s, util::shortcode(filter.action)},
        {u"match"_s, util::shortcode(filter.match)},
        {u"option"_s, util::shortcode(filter.option)},
        {u"anime"_s, animeIds},
        {u"conditions"_s, conditions},
    });
  }

  return array;
}

std::vector<Filter> FilterManager::fromJson(const QJsonArray& array) {
  std::vector<Filter> filters;

  for (const auto& value : array) {
    const auto object = value.toObject();

    Filter filter;
    filter.name = object[u"name"_s].toString().toStdString();
    filter.enabled = object[u"enabled"_s].toBool(true);
    filter.action =
        util::filterAction(object[u"action"_s].toString()).value_or(FilterAction::Discard);
    filter.match = util::filterMatch(object[u"match"_s].toString()).value_or(FilterMatch::All);
    filter.option =
        util::filterOption(object[u"option"_s].toString()).value_or(FilterOption::Default);

    for (const auto& id : object[u"anime"_s].toArray()) {
      filter.anime_ids.push_back(id.toInt());
    }

    for (const auto& item : object[u"conditions"_s].toArray()) {
      const auto conditionObject = item.toObject();
      const auto element = util::filterElement(conditionObject[u"element"_s].toString());
      const auto op = util::filterOperator(conditionObject[u"operator"_s].toString());

      // A condition Taiga cannot read would silently change what the filter does, so the whole
      // condition is skipped rather than guessed.
      if (!element || !op) continue;

      filter.conditions.push_back(
          {*element, *op, conditionObject[u"value"_s].toString().toStdString()});
    }

    filters.push_back(std::move(filter));
  }

  return filters;
}

void FilterManager::filter(Feed& feed) const {
  if (!akyuu::settings.torrentFilterEnabled()) return;

  // Preference filters compare an item against the rest of the feed, so they only make sense once
  // the regular filters have had their say. This is the order v1's `ExamineData()` uses.
  for (const bool preferences : {false, true}) {
    for (auto& item : feed.items) {
      for (const auto& filter : filters()) {
        if (preferences != (filter.action == FilterAction::Prefer)) continue;
        applyFilter(filter, feed, item, true);
      }
    }
  }
}

bool FilterManager::addDiscardFilter(const int animeId) {
  const auto anime = anime::db.item(animeId);

  if (!anime) return false;

  auto filters = this->filters();

  Filter filter;
  filter.name = QCoreApplication::translate("track", "Discard \"%1\"")
                    .arg(QString::fromStdString(anime::preferredTitle(*anime)))
                    .toStdString();
  filter.action = FilterAction::Discard;
  filter.match = FilterMatch::All;
  filter.option = FilterOption::Default;
  filter.enabled = true;
  filter.conditions.push_back(
      {FilterElement::MetaId, FilterOperator::Equals, numberValue(anime->id)});

  filters.push_back(std::move(filter));
  setFilters(filters);

  return true;
}

std::vector<std::string> FilterManager::fansubFilter(const int animeId) const {
  std::vector<std::string> groups;

  for (const auto& filter : filters()) {
    if (!std::ranges::contains(filter.anime_ids, animeId)) continue;

    for (const auto& condition : filter.conditions) {
      if (condition.element == FilterElement::EpisodeGroup) groups.push_back(condition.value);
    }
  }

  return groups;
}

bool FilterManager::setFansubFilter(const int animeId, const std::string& group,
                                    const std::string& videoResolution) {
  const auto findCondition = [](Filter& filter, const FilterElement element) {
    return std::ranges::find(filter.conditions, element, &FilterCondition::element);
  };

  auto filters = this->filters();

  // Check existing filters
  for (auto it = filters.begin(); it != filters.end(); ++it) {
    auto& filter = *it;

    const auto id = std::ranges::find(filter.anime_ids, animeId);
    if (id == filter.anime_ids.end()) continue;

    const auto groupCondition = findCondition(filter, FilterElement::EpisodeGroup);
    if (groupCondition == filter.conditions.end()) continue;

    if (filter.anime_ids.size() > 1) {
      // The filter covers other anime as well, so this one is taken out of it and gets a filter
      // of its own below.
      filter.anime_ids.erase(id);
      if (group.empty()) {
        setFilters(filters);
        return true;
      }
      break;
    }

    if (group.empty()) {
      filters.erase(it);
    } else {
      groupCondition->value = group;
      if (!videoResolution.empty()) {
        const auto resolutionCondition =
            findCondition(filter, FilterElement::EpisodeVideoResolution);
        if (resolutionCondition != filter.conditions.end()) {
          resolutionCondition->value = videoResolution;
        } else {
          filter.conditions.push_back(
              {FilterElement::EpisodeVideoResolution, FilterOperator::Equals, videoResolution});
        }
      }
    }

    setFilters(filters);
    return true;
  }

  if (group.empty()) return false;

  const auto anime = anime::db.item(animeId);

  if (!anime) return false;

  // Create new filter
  Filter filter;
  filter.name = QCoreApplication::translate("track", "[Fansub] %1")
                    .arg(QString::fromStdString(anime::preferredTitle(*anime)))
                    .toStdString();
  filter.action = FilterAction::Prefer;
  filter.match = FilterMatch::All;
  filter.option = FilterOption::Default;
  filter.enabled = true;
  filter.conditions.push_back({FilterElement::EpisodeGroup, FilterOperator::Equals, group});
  if (!videoResolution.empty()) {
    filter.conditions.push_back(
        {FilterElement::EpisodeVideoResolution, FilterOperator::Equals, videoResolution});
  }
  filter.anime_ids.push_back(animeId);

  filters.push_back(std::move(filter));
  setFilters(filters);

  return true;
}

}  // namespace track
