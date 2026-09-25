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

#include "seasons_widget.hpp"

#include <QActionGroup>
#include <QDate>
#include <QLabel>
#include <QToolBar>
#include <QToolButton>
#include <tuple>

#include "akyuu/session.hpp"
#include "gui/common/anime_list_context.hpp"
#include "gui/common/anime_list_view.hpp"
#include "gui/common/anime_list_view_cards.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/models/anime_list_proxy_model.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime.hpp"
#include "sync/anilist/anilist.hpp"
#include "sync/kitsu/kitsu.hpp"
#include "sync/myanimelist/myanimelist.hpp"
#include "sync/search_params.hpp"
#include "sync/service.hpp"

namespace gui {

namespace {

using namespace Qt::StringLiterals;

// How far back the season list goes. v1 offers every season it has data for; here the range is
// simply generated, because the service is asked for the season either way.
constexpr int kFirstYear = 1960;

sync::SearchParams seasonParams(const anime::Season season) {
  return {
      .year = static_cast<int>(season.year),
      .season = season.name,
  };
}

}  // namespace

SeasonsWidget::SeasonsWidget(QWidget* parent)
    : PageWidget(parent),
      m_season(akyuu::session.season()),
      m_model(new AnimeListModel(this)),
      m_proxyModel(new AnimeListProxyModel(this)),
      m_labelStatus(new QLabel(this)),
      m_groupMenu(new QMenu(this)),
      m_seasonMenu(new QMenu(this)),
      m_sortMenu(new QMenu(this)),
      m_viewMenu(new QMenu(this)) {
  m_proxyModel->setSourceModel(m_model);
  m_proxyModel->setGroupBy(akyuu::session.seasonsGroupBy());
  m_proxyModel->sort(akyuu::session.seasonsSortColumn(), akyuu::session.seasonsSortOrder());

  // Toolbar
  {
    m_actionSeason = new QAction(theme.getIcon("calendar_month"), tr("Select season"), this);
    const auto actionRefresh = new QAction(theme.getIcon("sync"), tr("Refresh data"), this);
    m_actionGroup = new QAction(theme.getIcon("lists"), tr("Group by"), this);
    const auto actionSort = new QAction(theme.getIcon("sort"), tr("Sort by"), this);
    const auto actionView = new QAction(theme.getIcon("grid_view"), tr("View"), this);

    // v1 puts refreshing between two separators (`dlg_season.cpp:84-92`), but here it ends up
    // wedged between the season button and the three menus, with no separator to lean on: not
    // every style draws `QToolBar::separator`, and Taiga's own stylesheet only applies under
    // Fusion. It leads the toolbar instead, where nothing else has to make room for it.
    m_toolbar->addAction(actionRefresh);

    const auto spacer = new QWidget(m_toolbar);
    spacer->setFixedWidth(12);
    m_toolbar->addWidget(spacer);

    m_toolbar->addAction(m_actionSeason);
    m_toolbar->addAction(m_actionGroup);
    m_toolbar->addAction(actionSort);
    m_toolbar->addAction(actionView);

    // v1 labels this button "Refresh data", but here the three menus next to it already carry
    // their current value in their text; adding a fourth label pushes Sort by and View into the
    // toolbar's overflow menu (measured). The icon keeps its tooltip.

    for (const auto& [action, menu] : {std::pair{m_actionSeason, m_seasonMenu},
                                       {m_actionGroup, m_groupMenu},
                                       {actionSort, m_sortMenu},
                                       {actionView, m_viewMenu}}) {
      const auto button = static_cast<QToolButton*>(m_toolbar->widgetForAction(action));
      button->setPopupMode(QToolButton::InstantPopup);
      button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
      button->setMenu(menu);
    }

    connect(actionRefresh, &QAction::triggered, this, &SeasonsWidget::refresh);
  }

  m_labelStatus->setEnabled(false);
  m_toolbarLayout->addWidget(m_labelStatus);
  m_toolbarLayout->addStretch();

  connect(m_groupMenu, &QMenu::aboutToShow, this, &SeasonsWidget::initGroupMenu);
  connect(m_seasonMenu, &QMenu::aboutToShow, this, &SeasonsWidget::initSeasonMenu);
  connect(m_sortMenu, &QMenu::aboutToShow, this, &SeasonsWidget::initSortMenu);
  connect(m_viewMenu, &QMenu::aboutToShow, this, &SeasonsWidget::initViewMenu);

  setGroupBy(akyuu::session.seasonsGroupBy());
  setViewMode(akyuu::session.seasonsViewMode());

  const QList<sync::Service*> services{
      sync::anilist::Service::instance(),
      sync::kitsu::Service::instance(),
      sync::myanimelist::Service::instance(),
  };
  for (auto* service : services) {
    connect(service, &sync::Service::searchCompleted, this,
            [this](const sync::SearchParams& params, const QList<int>& ids) {
              // A season arrives one page at a time, so the ids are added as they come.
              if (params != seasonParams(m_season)) return;
              m_model->addIds(ids);
              updateStatus();  // the season arrives one page at a time
            });
  }

  setSeason(m_season);
}

void SeasonsWidget::saveState() {
  akyuu::session.setSeason(m_season);
  akyuu::session.setSeasonsGroupBy(m_proxyModel->groupBy());
  akyuu::session.setSeasonsSortColumn(m_proxyModel->sortColumn());
  akyuu::session.setSeasonsSortOrder(m_proxyModel->sortOrder());
  akyuu::session.setSeasonsViewMode(m_viewMode);
}

void SeasonsWidget::setSeason(const anime::Season season) {
  m_season = season;
  m_actionSeason->setText(formatSeason(m_season));

  // The model holds every anime Taiga knows about; the proxy is what narrows it to one season.
  m_proxyModel->setYearFilter(static_cast<int>(m_season.year));
  m_proxyModel->setSeasonFilter(static_cast<int>(m_season.name));

  updateStatus();
  refresh();
}

void SeasonsWidget::refresh() {
  sync::search(seasonParams(m_season));
}

void SeasonsWidget::updateStatus() {
  m_labelStatus->setText(
      tr("%1 · %2 titles").arg(formatSeason(m_season)).arg(m_proxyModel->rowCount()));

  // The cards cannot carry a group header, so the breakdown goes in the tooltip. It does not go
  // in the label itself: with five groups the text is long enough to squeeze the toolbar into its
  // overflow menu.
  QStringList groups;
  for (const auto& [name, count] : m_proxyModel->groupCounts()) {
    groups.push_back(u"%1 %2"_s.arg(name).arg(count));
  }
  m_labelStatus->setToolTip(groups.join(u" · "_s));
}

void SeasonsWidget::initGroupMenu() {
  // v1 has the same three (`dlg_season.cpp`), minus the option to turn grouping off.
  const QList<QPair<QString, AnimeListGroupBy>> items{
      {tr("None"), AnimeListGroupBy::None},
      {tr("Airing status"), AnimeListGroupBy::AiringStatus},
      {tr("List status"), AnimeListGroupBy::ListStatus},
      {tr("Type"), AnimeListGroupBy::Type},
  };

  const auto actionGroup = new QActionGroup(this);

  m_groupMenu->clear();

  for (const auto& [text, groupBy] : items) {
    const auto action =
        m_groupMenu->addAction(text, this, [this, groupBy]() { setGroupBy(groupBy); });
    action->setCheckable(true);
    action->setChecked(groupBy == m_proxyModel->groupBy());
    actionGroup->addAction(action);
  }
}

void SeasonsWidget::setGroupBy(const AnimeListGroupBy groupBy) {
  m_proxyModel->setGroupBy(groupBy);
  updateStatus();
}

void SeasonsWidget::initSeasonMenu() {
  m_seasonMenu->clear();

  // No check marks here: the toolbar button already reads "Fall 2026", and a column of empty
  // checkboxes next to every season is noise. v1 keeps the radio marks for the menus that have
  // no such label (`SeasonGroup`, `SeasonSort`), which is what the other menus below do.
  const auto addSeason = [this](QMenu* menu, const anime::Season season) {
    menu->addAction(formatSeason(season), this, [this, season]() { setSeason(season); });
  };

  // Listing every season since 1960 in one menu would be hundreds of entries, so the recent ones
  // are up front and the rest live under their year.
  auto season = anime::Season{QDate::currentDate().toStdSysDays()};
  ++season;

  for (int i = 0; i < 6; ++i, --season) {
    addSeason(m_seasonMenu, season);
  }

  m_seasonMenu->addSeparator();

  for (int year = static_cast<int>(season.year); year >= kFirstYear; --year) {
    const auto menu = m_seasonMenu->addMenu(QString::number(year));
    for (const auto name : {anime::SeasonName::Winter, anime::SeasonName::Spring,
                            anime::SeasonName::Summer, anime::SeasonName::Fall}) {
      addSeason(menu, anime::Season{name, std::chrono::year{year}});
    }
  }
}

void SeasonsWidget::initSortMenu() {
  using Qt::SortOrder::AscendingOrder;
  using Qt::SortOrder::DescendingOrder;

  // v1 offers airing date, episodes, popularity, score and title. Episode count and popularity
  // have no column in v2's model, so they are left out rather than mislabelled.
  const QList<std::tuple<QString, AnimeListModel::Column, Qt::SortOrder>> items{
      {tr("Airing date"), AnimeListModel::COLUMN_STARTED, DescendingOrder},
      {tr("Score"), AnimeListModel::COLUMN_AVERAGE, DescendingOrder},
      {tr("Title"), AnimeListModel::COLUMN_TITLE, AscendingOrder},
  };

  const auto actionGroup = new QActionGroup(this);

  m_sortMenu->clear();

  for (const auto& [text, column, order] : items) {
    const auto action = m_sortMenu->addAction(text, this, [this, column, order]() {
      if (m_listView) {
        m_listView->sortByColumn(column, order);
      } else {
        m_proxyModel->sort(column, order);
      }
    });

    action->setCheckable(true);
    action->setChecked(column == m_proxyModel->sortColumn());
    actionGroup->addAction(action);
  }
}

void SeasonsWidget::initViewMenu() {
  static const QList<QPair<QString, ListViewMode>> items{
      {"List", ListViewMode::List},
      {"Cards", ListViewMode::Cards},
  };

  const auto actionGroup = new QActionGroup(this);

  m_viewMenu->clear();

  for (const auto& [text, mode] : items) {
    const auto action = m_viewMenu->addAction(text, this, [this, mode]() { setViewMode(mode); });
    action->setCheckable(true);
    action->setChecked(mode == m_viewMode);
    actionGroup->addAction(action);
  }
}

void SeasonsWidget::setViewMode(ListViewMode mode) {
  if (m_listView) {
    layout()->removeWidget(m_listView);
    m_listView->deleteLater();
    m_listView = nullptr;
  }
  if (m_listViewCards) {
    layout()->removeWidget(m_listViewCards);
    m_listViewCards->deleteLater();
    m_listViewCards = nullptr;
  }

  m_viewMode = mode;

  switch (mode) {
    case ListViewMode::List:
      m_listView = new ListView(this, m_model, m_proxyModel, AnimeListContext::Search);
      layout()->addWidget(m_listView);
      m_listView->show();
      break;

    case ListViewMode::Cards:
      m_listViewCards = new ListViewCards(this, m_model, m_proxyModel, AnimeListContext::Search);
      layout()->addWidget(m_listViewCards);
      m_listViewCards->show();
      break;
  }
}

}  // namespace gui
