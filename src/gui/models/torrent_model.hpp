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

#include <QAbstractItemModel>
#include <QList>
#include <array>
#include <string>

namespace track {
struct FeedItem;
}

namespace gui {

enum class TorrentItemDataRole {
  FeedItem = Qt::UserRole,
  SortValue,
};

// Items are grouped under a row per torrent category, as in v1.
class TorrentModel final : public QAbstractItemModel {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(TorrentModel)

public:
  enum Column {
    COLUMN_TITLE,
    COLUMN_EPISODE,
    COLUMN_GROUP,
    COLUMN_SIZE,
    COLUMN_VIDEO,
    COLUMN_SEEDERS,
    COLUMN_LEECHERS,
    COLUMN_DOWNLOADS,
    COLUMN_DESCRIPTION,
    COLUMN_FILENAME,
    COLUMN_DATE,
    NUM_COLUMNS,
  };

  TorrentModel(QObject* parent);
  ~TorrentModel() override = default;

  QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  int rowCount(const QModelIndex& parent = {}) const override;
  int columnCount(const QModelIndex& parent = {}) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  const track::FeedItem* itemAt(const QModelIndex& index) const;
  QList<const track::FeedItem*> checkedItems() const;

  // Right-click actions on the torrents page, as in v1: discard one item, every item of the given
  // anime, or the ones released by a different group than the one given.
  void discardItem(const QModelIndex& index);
  void discardItems(const int animeId);
  void discardOtherFansubs(const int animeId, const std::string& group,
                           const std::string& videoResolution);

private:
  static constexpr int kCategoryCount = 3;

  void refreshCategories();
  bool isCategory(const QModelIndex& index) const;
  track::FeedItem* mutableItemAt(const QModelIndex& index) const;

  // Indices into the feed's item list, one bucket per category
  std::array<QList<int>, kCategoryCount> m_categories;
};

}  // namespace gui
