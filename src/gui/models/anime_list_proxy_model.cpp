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

#include "anime_list_proxy_model.hpp"

#include <limits>
#include <ranges>

#include "akyuu/settings.hpp"
#include "base/string.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/utils/format.hpp"
#include "media/anime.hpp"
#include "media/anime_list.hpp"
#include "media/anime_list_utils.hpp"
#include "media/anime_season.hpp"
#include "media/anime_utils.hpp"
#include "track/library.hpp"

namespace {

const Anime* getAnime(const QModelIndex& index) {
  const int role = static_cast<int>(gui::AnimeListItemDataRole::Anime);
  return index.data(role).value<const Anime*>();
}

const ListEntry* getListEntry(const QModelIndex& index) {
  const int role = static_cast<int>(gui::AnimeListItemDataRole::ListEntry);
  return index.data(role).value<const ListEntry*>();
}

anime::list::Status listStatus(const ListEntry* entry) {
  if (!entry || entry->pending_delete) return anime::list::Status::NotInList;
  return entry->status;
}

// The key orders the groups. v1 lists them in enum order and keeps "not in list" last
// (`kNotInListGroupIndex`), so the list status key moves `NotInList` to the end.
int groupKey(const gui::AnimeListGroupBy groupBy, const Anime* anime, const ListEntry* entry) {
  switch (groupBy) {
    case gui::AnimeListGroupBy::AiringStatus:
      return static_cast<int>(anime->status);
    case gui::AnimeListGroupBy::ListStatus: {
      const auto status = listStatus(entry);
      return status == anime::list::Status::NotInList ? std::numeric_limits<int>::max()
                                                      : static_cast<int>(status);
    }
    case gui::AnimeListGroupBy::Type:
      return static_cast<int>(anime->type);
    case gui::AnimeListGroupBy::None:
      break;
  }
  return 0;
}

QString groupName(const gui::AnimeListGroupBy groupBy, const Anime* anime, const ListEntry* entry) {
  switch (groupBy) {
    case gui::AnimeListGroupBy::AiringStatus:
      return gui::formatStatus(anime->status);
    case gui::AnimeListGroupBy::ListStatus:
      return gui::formatListStatus(listStatus(entry));
    case gui::AnimeListGroupBy::Type:
      return gui::formatType(anime->type);
    case gui::AnimeListGroupBy::None:
      break;
  }
  return {};
}

}  // namespace

namespace gui {

AnimeListProxyModel::AnimeListProxyModel(QObject* parent) : QSortFilterProxyModel(parent) {
  setFilterKeyColumn(AnimeListModel::COLUMN_TITLE);
  setFilterRole(Qt::DisplayRole);

  setSortCaseSensitivity(Qt::CaseInsensitive);
  setSortRole(Qt::UserRole);

  // A scan changes which anime have a new episode, and with it the highlight and its place
  connect(track::library(), &track::Library::availabilityChanged, this, [this]() {
    if (akyuu::settings.listHighlightNewEpisodes()) invalidate();
  });
}

const AnimeListProxyModelFilter& AnimeListProxyModel::filters() const {
  return m_filter;
}

void AnimeListProxyModel::setFilters(const AnimeListProxyModelFilter& filters) {
  beginFilterChange();
  m_filter = filters;
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

AnimeListGroupBy AnimeListProxyModel::groupBy() const {
  return m_groupBy;
}

void AnimeListProxyModel::setGroupBy(const AnimeListGroupBy groupBy) {
  if (m_groupBy == groupBy) return;
  m_groupBy = groupBy;
  invalidate();
  sort(sortColumn(), sortOrder());
  emit groupByChanged();
}

// The status line reports how many items ended up in each group, in the order they are shown.
QList<QPair<QString, int>> AnimeListProxyModel::groupCounts() const {
  QList<QPair<QString, int>> counts;
  if (m_groupBy == AnimeListGroupBy::None) return counts;
  for (int row = 0; row < rowCount(); ++row) {
    const auto name =
        index(row, 0).data(static_cast<int>(AnimeListItemDataRole::GroupName)).toString();
    if (name.isEmpty()) continue;
    if (!counts.isEmpty() && counts.back().first == name) {
      ++counts.back().second;
    } else {
      counts.emplace_back(name, 1);
    }
  }
  return counts;
}

QVariant AnimeListProxyModel::data(const QModelIndex& index, const int role) const {
  switch (static_cast<AnimeListItemDataRole>(role)) {
    case AnimeListItemDataRole::GroupKey:
    case AnimeListItemDataRole::GroupName: {
      if (m_groupBy == AnimeListGroupBy::None) return {};
      const auto sourceIndex = mapToSource(index);
      const auto anime = getAnime(sourceIndex);
      if (!anime) return {};
      const auto entry = getListEntry(sourceIndex);
      if (static_cast<AnimeListItemDataRole>(role) == AnimeListItemDataRole::GroupKey) {
        return groupKey(m_groupBy, anime, entry);
      }
      return groupName(m_groupBy, anime, entry);
    }
    default:
      break;
  }

  return QSortFilterProxyModel::data(index, role);
}

void AnimeListProxyModel::setYearFilter(std::optional<int> year) {
  beginFilterChange();
  m_filter.year = year;
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void AnimeListProxyModel::setSeasonFilter(std::optional<int> season) {
  beginFilterChange();
  m_filter.season = season;
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void AnimeListProxyModel::setTypeFilter(std::optional<int> type) {
  beginFilterChange();
  m_filter.type = type;
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void AnimeListProxyModel::setStatusFilter(std::optional<int> status) {
  beginFilterChange();
  m_filter.status = status;
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void AnimeListProxyModel::setListStatusFilter(AnimeListStatusFilter filter) {
  beginFilterChange();
  m_filter.listStatus = filter;
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void AnimeListProxyModel::setTextFilter(const QString& text) {
  beginFilterChange();
  m_filter.text = text;
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool AnimeListProxyModel::filterAcceptsRow(int row, const QModelIndex& parent) const {
  const auto model = static_cast<AnimeListModel*>(sourceModel());
  if (!model) return false;
  const auto index = model->index(row, 0, parent);
  const auto anime = getAnime(index);
  if (!anime) return false;
  const auto entry = getListEntry(index);

  static const auto contains = [](const std::string& str, const QStringView view) {
    return QString::fromStdString(str).contains(view, Qt::CaseInsensitive);
  };

  static const auto list_contains = [](const std::vector<std::string>& list,
                                       const QStringView view) {
    return std::ranges::any_of(list,
                               [view](const std::string& str) { return contains(str, view); });
  };

  // Year
  if (m_filter.year) {
    if (anime->date_started.year() != *m_filter.year) return false;
  }

  // Season
  if (m_filter.season) {
    const anime::Season season{anime->date_started};
    if (static_cast<int>(season.name) != *m_filter.season) return false;
  }

  // Type
  if (m_filter.type) {
    if (static_cast<int>(anime->type) != *m_filter.type) return false;
  }

  // Status
  if (m_filter.status) {
    if (static_cast<int>(anime->status) != *m_filter.status) return false;
  }

  // List status
  if (m_filter.listStatus.status) {
    auto status = anime::list::Status::NotInList;
    if (entry && !entry->pending_delete) status = entry->status;
    if (m_filter.listStatus.anyStatus) {
      if (status == anime::list::Status::NotInList) return false;
    } else {
      if (static_cast<int>(status) != *m_filter.listStatus.status) return false;
    }
  }

  // Titles
  if (!m_filter.text.isEmpty()) {
    if (!contains(anime->titles.romaji, m_filter.text) &&
        !contains(anime->titles.english, m_filter.text) &&
        !contains(anime->titles.japanese, m_filter.text) &&
        !list_contains(anime->titles.synonyms, m_filter.text)) {
      return false;
    }
  }

  return true;
}

int AnimeListProxyModel::secondarySortColumn() const {
  return m_secondaryColumn;
}

Qt::SortOrder AnimeListProxyModel::secondarySortOrder() const {
  return m_secondaryOrder;
}

void AnimeListProxyModel::setSecondarySort(int column, Qt::SortOrder order) {
  m_secondaryColumn = column;
  m_secondaryOrder = order;
}

// As in v1, sorting by another column hands the previous one down as the secondary sort.
void AnimeListProxyModel::sort(int column, Qt::SortOrder order) {
  if (sortColumn() >= 0 && column != sortColumn()) {
    setSecondarySort(sortColumn(), sortOrder());
  }
  QSortFilterProxyModel::sort(column, order);
}

bool AnimeListProxyModel::lessThan(const QModelIndex& lhs, const QModelIndex& rhs) const {
  const auto lhs_anime = getAnime(lhs);
  const auto rhs_anime = getAnime(rhs);
  if (!lhs_anime || !rhs_anime) return false;

  const auto lhs_entry = getListEntry(lhs);
  const auto rhs_entry = getListEntry(rhs);

  // The group comes first, so that sorting orders the items within their group. The groups
  // themselves always read in enum order, whichever way the sort order points.
  if (m_groupBy != AnimeListGroupBy::None) {
    const auto lhs_group = groupKey(m_groupBy, lhs_anime, lhs_entry);
    const auto rhs_group = groupKey(m_groupBy, rhs_anime, rhs_entry);
    if (lhs_group != rhs_group) {
      return sortOrder() == Qt::AscendingOrder ? lhs_group < rhs_group : lhs_group > rhs_group;
    }
  }

  // v1 can keep the highlighted anime above the rest, whichever way the list is sorted
  if (akyuu::settings.listHighlightNewEpisodes() && akyuu::settings.listHighlightedOnTop()) {
    const bool lhs_new = hasNewEpisode(*lhs_anime, lhs_entry);
    const bool rhs_new = hasNewEpisode(*rhs_anime, rhs_entry);
    if (lhs_new != rhs_new) return sortOrder() == Qt::AscendingOrder ? lhs_new : rhs_new;
  }

  const auto column = lhs.column();

  if (lessThanByColumn(column, lhs, rhs)) return true;
  if (lessThanByColumn(column, rhs, lhs)) return false;

  // v1's `sort/column2`: rows that tie on the sorted column keep the order of the column that was
  // sorted before. It has its own direction, while Qt reverses this whole comparison for a
  // descending primary sort, so the operands are swapped when the two directions differ.
  if (m_secondaryColumn < 0 || m_secondaryColumn == column) return false;

  const auto lhs2 = lhs.siblingAtColumn(m_secondaryColumn);
  const auto rhs2 = rhs.siblingAtColumn(m_secondaryColumn);
  return m_secondaryOrder == sortOrder() ? lessThanByColumn(m_secondaryColumn, lhs2, rhs2)
                                         : lessThanByColumn(m_secondaryColumn, rhs2, lhs2);
}

bool AnimeListProxyModel::lessThanByColumn(int column, const QModelIndex& lhs,
                                           const QModelIndex& rhs) const {
  const auto lhs_anime = getAnime(lhs);
  const auto rhs_anime = getAnime(rhs);
  if (!lhs_anime || !rhs_anime) return false;

  const auto lhs_entry = getListEntry(lhs);
  const auto rhs_entry = getListEntry(rhs);

  switch (column) {
    case AnimeListModel::COLUMN_TITLE:
      return compareStrings(anime::preferredTitle(*lhs_anime), anime::preferredTitle(*rhs_anime),
                            Qt::CaseInsensitive) < 0;

    case AnimeListModel::COLUMN_DURATION:
      return lhs_anime->episode_length < rhs_anime->episode_length;

    case AnimeListModel::COLUMN_AVERAGE:
      return lhs_anime->score < rhs_anime->score;

    case AnimeListModel::COLUMN_TYPE:
      return lhs_anime->type < rhs_anime->type;

    case AnimeListModel::COLUMN_PROGRESS: {
      const auto lhs_ratio = anime::list::getProgressRatio(lhs_anime, lhs_entry);
      const auto rhs_ratio = anime::list::getProgressRatio(rhs_anime, rhs_entry);
      if (lhs_ratio != rhs_ratio) return lhs_ratio < rhs_ratio;
      return anime::estimateEpisodeCount(*lhs_anime, 0) <
             anime::estimateEpisodeCount(*rhs_anime, 0);
    }

    case AnimeListModel::COLUMN_REWATCHES:
      return (lhs_entry ? lhs_entry->rewatched_times : 0) <
             (rhs_entry ? rhs_entry->rewatched_times : 0);

    case AnimeListModel::COLUMN_SCORE:
      return (lhs_entry ? lhs_entry->score : 0) < (rhs_entry ? rhs_entry->score : 0);

    case AnimeListModel::COLUMN_SEASON:
      return lhs_anime->date_started < rhs_anime->date_started;

    case AnimeListModel::COLUMN_STARTED:
      return (lhs_entry ? lhs_entry->date_started : FuzzyDate{}) <
             (rhs_entry ? rhs_entry->date_started : FuzzyDate{});

    case AnimeListModel::COLUMN_COMPLETED:
      return (lhs_entry ? lhs_entry->date_completed : FuzzyDate{}) <
             (rhs_entry ? rhs_entry->date_completed : FuzzyDate{});

    case AnimeListModel::COLUMN_LAST_UPDATED:
      return (lhs_entry ? lhs_entry->last_updated : 0) < (rhs_entry ? rhs_entry->last_updated : 0);

    case AnimeListModel::COLUMN_NOTES:
      return (lhs_entry ? lhs_entry->notes : "") < (rhs_entry ? rhs_entry->notes : "");
  }

  return false;
}

}  // namespace gui
