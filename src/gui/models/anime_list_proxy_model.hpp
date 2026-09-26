/**
 * Akyuu
 * Copyright (C) 2010-2024, Eren Okka
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

#include <QList>
#include <QSortFilterProxyModel>
#include <optional>

namespace gui {

// v1 groups the Seasons page by airing status, list status or type (`dlg_season.cpp`). There the
// grouping is a feature of the Win32 list view; here it is a sort key, so that both the list and
// the cards keep the items of a group together.
enum class AnimeListGroupBy {
  None,
  AiringStatus,
  ListStatus,
  Type,
};

struct AnimeListStatusFilter {
  std::optional<int> status;
  bool anyStatus = false;
};

struct AnimeListProxyModelFilter {
  std::optional<int> year;
  std::optional<int> season;
  std::optional<int> type;
  std::optional<int> status;
  AnimeListStatusFilter listStatus;
  QString text;
};

class AnimeListProxyModel final : public QSortFilterProxyModel {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(AnimeListProxyModel)

public:
  AnimeListProxyModel(QObject* parent);
  ~AnimeListProxyModel() = default;

  const AnimeListProxyModelFilter& filters() const;
  void setFilters(const AnimeListProxyModelFilter& filters);

  AnimeListGroupBy groupBy() const;
  void setGroupBy(AnimeListGroupBy groupBy);
  QList<QPair<QString, int>> groupCounts() const;

  void setYearFilter(std::optional<int> year);
  void setSeasonFilter(std::optional<int> season);
  void setTypeFilter(std::optional<int> type);
  void setStatusFilter(std::optional<int> status);
  void setListStatusFilter(AnimeListStatusFilter filter);
  void setTextFilter(const QString& text);

  QVariant data(const QModelIndex& index, int role) const override;

  int secondarySortColumn() const;
  Qt::SortOrder secondarySortOrder() const;
  void setSecondarySort(int column, Qt::SortOrder order);
  void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

signals:
  void groupByChanged();

protected:
  bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
  bool lessThan(const QModelIndex& lhs, const QModelIndex& rhs) const override;

private:
  bool lessThanByColumn(int column, const QModelIndex& lhs, const QModelIndex& rhs) const;

  AnimeListProxyModelFilter m_filter;
  AnimeListGroupBy m_groupBy = AnimeListGroupBy::None;
  int m_secondaryColumn = -1;
  Qt::SortOrder m_secondaryOrder = Qt::AscendingOrder;
};

}  // namespace gui
