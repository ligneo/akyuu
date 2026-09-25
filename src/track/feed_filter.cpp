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

#include "feed_filter.hpp"

#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <map>
#include <ranges>

#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "media/anime_utils.hpp"
#include "track/library.hpp"

namespace track {

namespace {

using namespace Qt::StringLiterals;

// v1 joins multi-valued elements with a space, and filter values are written to match.
QString joinElements(const std::vector<std::string>& values) {
  QStringList list;
  for (const auto& value : values) list.append(QString::fromStdString(value));
  return list.join(u' ');
}

// v1's `GetVideoResolutionHeight()`. Comparing resolutions as text would order 720p above 1080p.
int videoResolutionHeight(const QString& value) {
  static const QRegularExpression dimensions{u"^(\\d+)x(\\d+)$"_s};
  static const QRegularExpression height{u"^(\\d+)p$"_s};

  if (const auto match = dimensions.match(value); match.hasMatch()) {
    return match.captured(2).toInt();
  }

  if (const auto match = height.match(value); match.hasMatch()) {
    return match.captured(1).toInt();
  }

  // A bare number is only a resolution when it is one of the usual ones, as in v1.
  switch (const auto number = value.toInt()) {
    case 480:
    case 720:
    case 1080:
      return number;
  }

  return 0;
}

// v1 runs condition values through its whole script engine. Only the variables that make sense in
// a filter are recognized here; the default presets need `%watched%`, and the rest come from the
// item itself. As in v1, a variable with no value becomes an empty string, which keeps a condition
// such as "episode number <= %watched%" from matching when nothing has been watched yet.
QString replaceVariables(const QString& value, const Episode& episode) {
  if (!value.contains(u'%')) return value;

  const auto number = [](const int value) {
    return value > 0 ? QString::number(value) : QString{};
  };

  std::map<QString, QString> fields{
      {u"episode"_s, {}}, {u"group"_s, {}}, {u"id"_s, {}},         {u"name"_s, {}},
      {u"notes"_s, {}},   {u"score"_s, {}}, {u"resolution"_s, {}}, {u"status"_s, {}},
      {u"title"_s, {}},   {u"total"_s, {}}, {u"version"_s, {}},    {u"video"_s, {}},
      {u"watched"_s, {}},
  };

  const auto item = anime::db.item(episode.animeId());
  const auto entry = anime::db.entry(episode.animeId());

  fields[u"title"_s] = QString::fromStdString(item ? anime::preferredTitle(*item)
                                                   : episode.element(anitomy::ElementKind::Title));
  fields[u"group"_s] = QString::fromStdString(episode.element(anitomy::ElementKind::ReleaseGroup));
  fields[u"name"_s] = QString::fromStdString(episode.element(anitomy::ElementKind::EpisodeTitle));
  fields[u"resolution"_s] =
      QString::fromStdString(episode.element(anitomy::ElementKind::VideoResolution));
  fields[u"version"_s] = QString::fromStdString(
      episode.element(anitomy::ElementKind::ReleaseVersion, std::string{"1"}));
  fields[u"video"_s] = joinElements(episode.elements(anitomy::ElementKind::VideoTerm));

  if (const auto range = episode.episodeNumberRange()) {
    fields[u"episode"_s] = QString::number(range->second);
  }

  if (item) {
    fields[u"id"_s] = number(item->id);
    fields[u"total"_s] = number(item->episode_count);
  }

  if (entry) {
    fields[u"notes"_s] = QString::fromStdString(entry->notes);
    fields[u"score"_s] = number(entry->score);
    fields[u"status"_s] = QString::number(static_cast<int>(entry->status));
    fields[u"watched"_s] = number(entry->watched_episodes);
  }

  auto result = value;

  for (const auto& [name, field] : fields) {
    result.replace(u"%%1%"_s.arg(name), field);
  }

  return result;
}

QString conditionElement(const FilterElement element, const FeedItem& item) {
  const auto anime = anime::db.item(item.episode.animeId());
  const auto entry = anime::db.entry(item.episode.animeId());

  const auto episodeHigh = [&item]() -> std::optional<int> {
    const auto range = item.episode.episodeNumberRange();
    return range ? std::optional{range->second} : std::nullopt;
  };

  switch (element) {
    case FilterElement::FileTitle:
      return QString::fromStdString(item.title);
    case FilterElement::FileCategory:
      return torrentCategoryName(item.torrent_category);
    case FilterElement::FileDescription:
      return QString::fromStdString(item.description);
    case FilterElement::FileLink:
      return QString::fromStdString(item.link);
    case FilterElement::FileSize:
      return QString::number(item.file_size);
    case FilterElement::MetaId:
      return QString::number(anime ? anime->id : anime::kUnknownId);
    case FilterElement::EpisodeTitle:
      return QString::fromStdString(item.episode.element(anitomy::ElementKind::Title));
    case FilterElement::MetaDateStart:
      return anime ? QString::fromStdString(anime->date_started.to_string()) : QString{};
    case FilterElement::MetaDateEnd:
      return anime ? QString::fromStdString(anime->date_finished.to_string()) : QString{};
    case FilterElement::MetaEpisodes:
      return anime ? QString::number(anime->episode_count) : QString{};
    case FilterElement::MetaStatus:
      return QString::number(
          static_cast<int>(anime ? anime::airingStatus(*anime) : anime::Status::Unknown));
    case FilterElement::MetaType:
      return QString::number(static_cast<int>(anime ? anime->type : anime::Type::Unknown));
    case FilterElement::UserNotes:
      return entry ? QString::fromStdString(entry->notes) : QString{};
    case FilterElement::UserStatus:
      return QString::number(
          static_cast<int>(entry ? entry->status : anime::list::Status::NotInList));
    case FilterElement::EpisodeNumber:
      // An item with no episode number covers the whole series, as in v1.
      if (const auto number = episodeHigh()) return QString::number(*number);
      return anime ? QString::number(anime->episode_count) : QString{};
    case FilterElement::EpisodeVersion:
      return QString::fromStdString(
          item.episode.element(anitomy::ElementKind::ReleaseVersion, std::string{"1"}));
    case FilterElement::LocalEpisodeAvailable:
      if (!anime) return {};
      if (const auto number = episodeHigh()) {
        return QString::number(library()->isEpisodeAvailable(anime->id, *number));
      }
      return u"0"_s;
    case FilterElement::EpisodeGroup:
      return QString::fromStdString(item.episode.element(anitomy::ElementKind::ReleaseGroup));
    case FilterElement::EpisodeVideoResolution:
      return QString::fromStdString(item.episode.element(anitomy::ElementKind::VideoResolution));
    case FilterElement::EpisodeVideoType:
      return joinElements(item.episode.elements(anitomy::ElementKind::VideoTerm));
  }

  return {};
}

bool isNumericElement(const FilterElement element) {
  switch (element) {
    case FilterElement::FileSize:
    case FilterElement::MetaId:
    case FilterElement::MetaEpisodes:
    case FilterElement::MetaStatus:
    case FilterElement::MetaType:
    case FilterElement::UserStatus:
    case FilterElement::EpisodeNumber:
    case FilterElement::EpisodeVersion:
    case FilterElement::LocalEpisodeAvailable:
      return true;
    default:
      return false;
  }
}

template <typename T>
bool applyOperator(const T& a, const T& b, const FilterOperator op) {
  switch (op) {
    default:
    case FilterOperator::Equals:
      return a == b;
    case FilterOperator::NotEquals:
      return a != b;
    case FilterOperator::IsGreaterThan:
      return a > b;
    case FilterOperator::IsGreaterThanOrEqualTo:
      return a >= b;
    case FilterOperator::IsLessThan:
      return a < b;
    case FilterOperator::IsLessThanOrEqualTo:
      return a <= b;
  }
}

bool evaluateCondition(const FilterCondition& condition, const FeedItem& item) {
  const auto element = conditionElement(condition.element, item);
  const auto value = replaceVariables(QString::fromStdString(condition.value), item.episode);

  // An empty side makes a numeric comparison meaningless, so both are compared as text instead.
  // See taiga#639.
  const auto isNumeric =
      !element.isEmpty() && !value.isEmpty() && isNumericElement(condition.element);

  switch (condition.op) {
    case FilterOperator::Equals:
    case FilterOperator::NotEquals:
    case FilterOperator::IsGreaterThan:
    case FilterOperator::IsGreaterThanOrEqualTo:
    case FilterOperator::IsLessThan:
    case FilterOperator::IsLessThanOrEqualTo:
      switch (condition.element) {
        case FilterElement::FileSize:
          return applyOperator(element.toULongLong(), parseSizeString(value), condition.op);
        case FilterElement::EpisodeVideoResolution:
          return applyOperator(videoResolutionHeight(element), videoResolutionHeight(value),
                               condition.op);
        default:
          break;
      }
      if (isNumeric) {
        if (condition.op == FilterOperator::Equals || condition.op == FilterOperator::NotEquals) {
          // The availability element is a flag, so v1 lets it be written out.
          if (value.compare(u"true"_s, Qt::CaseInsensitive) == 0) {
            return applyOperator(element.toInt(), 1, condition.op);
          }
        }
        return applyOperator(element.toInt(), value.toInt(), condition.op);
      }
      if (condition.op == FilterOperator::Equals || condition.op == FilterOperator::NotEquals) {
        return applyOperator(element.compare(value, Qt::CaseInsensitive) == 0, true, condition.op);
      }
      return applyOperator(element.compare(value, Qt::CaseInsensitive), 0, condition.op);

    case FilterOperator::BeginsWith:
      return element.startsWith(value, Qt::CaseInsensitive);
    case FilterOperator::EndsWith:
      return element.endsWith(value, Qt::CaseInsensitive);
    case FilterOperator::Contains:
      return element.contains(value, Qt::CaseInsensitive);
    case FilterOperator::NotContains:
      return !element.contains(value, Qt::CaseInsensitive);
  }

  return false;
}

FeedItemState discardedState(const FilterOption option) {
  switch (option) {
    case FilterOption::Deactivate:
      return FeedItemState::DiscardedInactive;
    case FilterOption::Hide:
      return FeedItemState::DiscardedHidden;
    default:
      return FeedItemState::DiscardedNormal;
  }
}

bool matchesConditions(const Filter& filter, const FeedItem& item) {
  const auto matches = [&item](const FilterCondition& condition) {
    return evaluateCondition(condition, item);
  };

  switch (filter.match) {
    case FilterMatch::Any:
      return std::ranges::any_of(filter.conditions, matches);
    default:
      return std::ranges::all_of(filter.conditions, matches);
  }
}

}  // namespace

bool applyFilter(const Filter& filter, Feed& feed, FeedItem& item, const bool recursive) {
  if (!filter.enabled) return false;

  // No need to filter if the item was discarded before
  if (item.isDiscarded()) return false;

  if (!filter.anime_ids.empty()) {
    if (!std::ranges::contains(filter.anime_ids, item.episode.animeId())) {
      return false;  // Filter doesn't apply to this item
    }
  }

  const bool matched = matchesConditions(filter, item);

  switch (filter.action) {
    case FilterAction::Discard:
      if (!matched) return false;
      // Discard matched items, regardless of their previous state
      item.state = discardedState(filter.option);
      break;

    case FilterAction::Select:
      if (!matched) return false;
      // Select matched items, if they were not discarded before
      item.state = FeedItemState::Selected;
      break;

    case FilterAction::Prefer:
      if (!recursive) {
        // A preference filter matched an item before, and the rest of the feed is now checked for
        // mismatches. Whether the preference is weak or strong no longer matters here.
        if (matched) return false;
        item.state = discardedState(filter.option);
        break;
      }
      // Filters are strong if they're limited, weak otherwise
      if (!filter.anime_ids.empty()) {
        item.state = matched ? FeedItemState::Selected : discardedState(filter.option);
        break;
      }
      if (!matched) return false;
      if (!applyPreferenceFilter(filter, feed, item)) return false;
      break;
  }

  return true;
}

bool applyPreferenceFilter(const Filter& filter, Feed& feed, FeedItem& item) {
  std::map<FilterElement, bool> elementFound;

  for (const auto& condition : filter.conditions) {
    switch (condition.element) {
      case FilterElement::MetaId:
      case FilterElement::EpisodeTitle:
      case FilterElement::EpisodeNumber:
      case FilterElement::EpisodeGroup:
        elementFound[condition.element] = true;
        break;
      default:
        break;
    }
  }

  bool filterApplied = false;

  for (auto& feedItem : feed.items) {
    // Do not bother if the item was discarded before
    if (feedItem.isDiscarded()) continue;
    // Do not filter the same item again
    if (&feedItem == &item) continue;

    // Is it the same title/anime?
    if (feedItem.episode.animeId() == anime::kUnknownId &&
        item.episode.animeId() == anime::kUnknownId) {
      if (!elementFound[FilterElement::EpisodeTitle]) {
        if (compareStrings(feedItem.episode.element(anitomy::ElementKind::Title),
                           item.episode.element(anitomy::ElementKind::Title),
                           Qt::CaseInsensitive) != 0) {
          continue;
        }
      }
    } else {
      if (!elementFound[FilterElement::MetaId]) {
        if (feedItem.episode.animeId() != item.episode.animeId()) continue;
      }
    }
    // Is it the same episode?
    if (!elementFound[FilterElement::EpisodeNumber]) {
      if (feedItem.episode.episodeNumberRange() != item.episode.episodeNumberRange()) continue;
    }
    // Is it from the same fansub group?
    if (!elementFound[FilterElement::EpisodeGroup]) {
      if (compareStrings(feedItem.episode.element(anitomy::ElementKind::ReleaseGroup),
                         item.episode.element(anitomy::ElementKind::ReleaseGroup),
                         Qt::CaseInsensitive) != 0) {
        continue;
      }
    }

    // Try applying the same filter
    if (applyFilter(filter, feed, feedItem, false)) filterApplied = true;
  }

  return filterApplied;
}

}  // namespace track
