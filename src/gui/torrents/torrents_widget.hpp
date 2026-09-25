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

#include <QSortFilterProxyModel>
#include <QTreeView>

#include "gui/common/page_widget.hpp"

namespace gui {

class TorrentModel;

class TorrentsWidget final : public PageWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(TorrentsWidget)

public:
  TorrentsWidget(QWidget* parent);
  ~TorrentsWidget() override = default;

public slots:
  void setFilterText(const QString& text);
  void search(const QString& title);

private:
  void initToolbar();
  void openItemLink(const QModelIndex& index) const;
  void showContextMenu();

  QAction* m_actionRefresh = nullptr;
  TorrentModel* m_model = nullptr;
  QSortFilterProxyModel* m_proxyModel = nullptr;
  QTreeView* m_view = nullptr;
};

}  // namespace gui
