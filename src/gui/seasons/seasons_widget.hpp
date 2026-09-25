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

#include <QMenu>

#include "gui/common/anime_list_view_base.hpp"
#include "gui/common/page_widget.hpp"
#include "media/anime_season.hpp"

class QLabel;

namespace gui {

enum class AnimeListGroupBy;

class AnimeListModel;
class AnimeListProxyModel;
class ListView;
class ListViewCards;

// v1's Seasons page. The list, the cards and the season request are the same ones the search page
// uses; what this adds is a season to browse and the toolbar that goes with it.
class SeasonsWidget final : public PageWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(SeasonsWidget)

public:
  SeasonsWidget(QWidget* parent);
  ~SeasonsWidget() = default;

  void saveState();

private:
  void initGroupMenu();
  void initSeasonMenu();
  void initSortMenu();
  void initViewMenu();
  void setGroupBy(AnimeListGroupBy groupBy);
  void setViewMode(ListViewMode mode);
  void setSeason(const anime::Season season);
  void refresh();
  void updateStatus();

  anime::Season m_season;
  AnimeListModel* m_model = nullptr;
  AnimeListProxyModel* m_proxyModel = nullptr;
  ListView* m_listView = nullptr;
  ListViewCards* m_listViewCards = nullptr;
  ListViewMode m_viewMode = ListViewMode::Cards;
  QAction* m_actionGroup = nullptr;
  QAction* m_actionSeason = nullptr;
  QLabel* m_labelStatus = nullptr;
  QMenu* m_groupMenu = nullptr;
  QMenu* m_seasonMenu = nullptr;
  QMenu* m_sortMenu = nullptr;
  QMenu* m_viewMenu = nullptr;
};

}  // namespace gui
