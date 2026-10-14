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

#pragma once

#include <QAbstractItemView>
#include <QObject>
#include <QTreeWidget>
#include <string>

#include "gui/common/anime_list_context.hpp"

namespace gui {

class AnimeListModel;
class AnimeListProxyModel;
class MediaMenu;

enum class ListViewMode {
  List,
  Cards,
};

class ListViewBase final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ListViewBase)

public:
  ListViewBase(QWidget* parent, QAbstractItemView* view, AnimeListModel* model,
               AnimeListProxyModel* proxyModel, AnimeListContext context);
  ~ListViewBase() = default;

  AnimeListContext context() const {
    return m_context;
  }

  bool handleKeyPress(const QKeyEvent* event);
  // Ctrl++ zooms the page, but on layouts that type + without Shift (German) it is v1's key for
  // the next episode, which the list keeps
  static bool keepsShortcut(const QKeyEvent* event);

public slots:
  void filterByText(const QString& text);
  void openAnimePage(const QModelIndex& index);
  void playNextEpisode(const QModelIndex& index);
  void showMediaDialog(const QModelIndex& index);
  void triggerClickAction(const QModelIndex& index, const std::string& action);
  void showMediaMenu();
  void updateSelectionStatus(const QItemSelection& selected, const QItemSelection& deselected);

private:
  MediaMenu* createMediaMenu();
  void changeEpisode(const QModelIndex& index, const int step);
  QModelIndexList selectedIndexes();

  QAbstractItemView* m_view = nullptr;
  AnimeListModel* m_model = nullptr;
  AnimeListProxyModel* m_proxyModel = nullptr;
  const AnimeListContext m_context;
};

}  // namespace gui
