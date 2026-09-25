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

#include "list_widget.hpp"

#include <QActionGroup>
#include <QListView>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolBar>
#include <QToolButton>
#include <algorithm>
#include <format>

#include "akyuu/session.hpp"
#include "base/string.hpp"
#include "gui/common/anime_list_context.hpp"
#include "gui/common/anime_list_view.hpp"
#include "gui/common/anime_list_view_cards.hpp"
#include "gui/main/main_window.hpp"
#include "gui/main/navigation_item_delegate.hpp"
#include "gui/main/navigation_widget.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/models/anime_list_proxy_model.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/theme.hpp"
#include "ui_main_window.h"

namespace gui {

using namespace Qt::StringLiterals;

ListWidget::ListWidget(QWidget* parent)
    : PageWidget(parent),
      m_model(new AnimeListModel(this)),
      m_proxyModel(new AnimeListProxyModel(this)),
      m_sortMenu(new QMenu(this)),
      m_viewMenu(new QMenu(this)),
      m_moreMenu(new QMenu(this)) {
  m_proxyModel->sort(akyuu::session.animeListSortColumn(), akyuu::session.animeListSortOrder());

  initToolbar();
  initTabs();
  setViewMode(akyuu::session.animeListViewMode());

  connect(m_sortMenu, &QMenu::aboutToShow, this, &ListWidget::initSortMenu);
  connect(m_viewMenu, &QMenu::aboutToShow, this, &ListWidget::initViewMenu);
  connect(m_moreMenu, &QMenu::aboutToShow, this, &ListWidget::initMoreMenu);

  connect(mainWindow()->navigation(), &NavigationWidget::currentListStatusChanged, this,
          [this](anime::list::Status status) {
            // The "Anime List" item itself opens the tab that was last shown, as in v1.
            if (!static_cast<int>(status)) {
              const auto navigation = mainWindow()->navigation();
              const auto tabStatus =
                  m_tabs->tabData(std::max(0, m_tabs->currentIndex())).value<anime::list::Status>();
              if (const auto item = navigation->findListStatusItem(tabStatus)) {
                navigation->setCurrentItem(item);
              }
              return;
            }
            {
              const QSignalBlocker blocker(m_tabs);
              for (int i = 0; i < m_tabs->count(); ++i) {
                if (m_tabs->tabData(i).value<anime::list::Status>() == status) {
                  m_tabs->setCurrentIndex(i);
                }
              }
            }
            m_proxyModel->setListStatusFilter({
                .status = static_cast<int>(status),
                .anyStatus = false,
            });
          });
}

// v1's tabs above the list, one per status with its count
void ListWidget::initTabs() {
  m_tabs = new QTabBar(this);
  m_tabs->setDrawBase(false);
  m_tabs->setExpanding(false);
  m_tabs->setDocumentMode(true);

  for (const auto status : anime::list::kStatuses) {
    const auto index = m_tabs->addTab(formatListStatus(status));
    m_tabs->setTabData(index, QVariant::fromValue(status));
  }

  m_toolbarLayout->insertWidget(0, m_tabs);

  connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
    const auto status = m_tabs->tabData(index).value<anime::list::Status>();
    const auto navigation = mainWindow()->navigation();
    if (const auto item = navigation->findListStatusItem(status)) {
      navigation->setCurrentItem(item);
    }
  });

  connect(mainWindow()->navigation(), &NavigationWidget::refreshed, this, &ListWidget::refreshTabs);
  refreshTabs();
}

void ListWidget::refreshTabs() {
  const auto navigation = mainWindow()->navigation();

  for (int i = 0; i < m_tabs->count(); ++i) {
    const auto status = m_tabs->tabData(i).value<anime::list::Status>();
    const auto item = navigation->findListStatusItem(status);
    const auto count =
        item ? item->data(0, static_cast<int>(NavigationItemDataRole::Counter)).toInt() : 0;
    m_tabs->setTabText(i, u"%1 (%2)"_s.arg(formatListStatus(status)).arg(count));
  }
}

ListViewMode ListWidget::viewMode() const {
  return m_viewMode;
}

void ListWidget::setViewMode(ListViewMode mode) {
  if (m_listView) {
    layout()->removeWidget(m_listView);
    m_listView->deleteLater();
    m_listView = nullptr;
  };
  if (m_listViewCards) {
    layout()->removeWidget(m_listViewCards);
    m_listViewCards->deleteLater();
    m_listViewCards = nullptr;
  };

  m_viewMode = mode;

  switch (mode) {
    case ListViewMode::List:
      m_listView = new ListView(this, m_model, m_proxyModel, AnimeListContext::List);
      layout()->addWidget(m_listView);
      m_listView->show();
      break;

    case ListViewMode::Cards:
      m_listViewCards = new ListViewCards(this, m_model, m_proxyModel, AnimeListContext::List);
      layout()->addWidget(m_listViewCards);
      m_listViewCards->show();
      break;
  }
}

void ListWidget::saveState() {
  akyuu::session.setAnimeListSortColumn(m_proxyModel->sortColumn());
  akyuu::session.setAnimeListSortOrder(m_proxyModel->sortOrder());
  akyuu::session.setAnimeListViewMode(m_viewMode);
}

void ListWidget::initToolbar() {
  const auto actionSort = new QAction(theme.getIcon("sort"), tr("Sort"), this);
  const auto actionView = new QAction(theme.getIcon("grid_view"), tr("View"), this);
  const auto actionMore = new QAction(theme.getIcon("more_horiz"), tr("More"), this);

  m_toolbar->addAction(actionSort);
  m_toolbar->addAction(actionView);
  m_toolbar->addAction(actionMore);

  const auto sortButton = static_cast<QToolButton*>(m_toolbar->widgetForAction(actionSort));
  sortButton->setPopupMode(QToolButton::InstantPopup);
  sortButton->setMenu(m_sortMenu);

  const auto viewButton = static_cast<QToolButton*>(m_toolbar->widgetForAction(actionView));
  viewButton->setPopupMode(QToolButton::InstantPopup);
  viewButton->setMenu(m_viewMenu);

  const auto moreButton = static_cast<QToolButton*>(m_toolbar->widgetForAction(actionMore));
  moreButton->setPopupMode(QToolButton::InstantPopup);
  moreButton->setMenu(m_moreMenu);
}

void ListWidget::initSortMenu() {
  using Qt::SortOrder::AscendingOrder;
  using Qt::SortOrder::DescendingOrder;

  static const QList<QPair<AnimeListModel::Column, Qt::SortOrder>> items{
      {AnimeListModel::COLUMN_TITLE, AscendingOrder},
      {AnimeListModel::COLUMN_PROGRESS, DescendingOrder},
      {AnimeListModel::COLUMN_DURATION, DescendingOrder},
      {AnimeListModel::COLUMN_REWATCHES, DescendingOrder},
      {AnimeListModel::COLUMN_SCORE, DescendingOrder},
      {AnimeListModel::COLUMN_AVERAGE, DescendingOrder},
      {AnimeListModel::COLUMN_TYPE, AscendingOrder},
      {AnimeListModel::COLUMN_SEASON, DescendingOrder},
      {AnimeListModel::COLUMN_STARTED, DescendingOrder},
      {AnimeListModel::COLUMN_COMPLETED, DescendingOrder},
      {AnimeListModel::COLUMN_LAST_UPDATED, DescendingOrder},
      {AnimeListModel::COLUMN_NOTES, AscendingOrder},
  };

  const auto actionGroup = new QActionGroup(this);

  m_sortMenu->clear();

  for (const auto& [column, order] : items) {
    const auto headerData =
        m_model->headerData(column, Qt::Orientation::Horizontal, Qt::DisplayRole);

    const auto action = m_sortMenu->addAction(headerData.toString(), this, [this, column, order]() {
      if (m_listView) {
        // Sorting the proxy model doesn't update the sort indicator on the header.
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

void ListWidget::initViewMenu() {
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

void ListWidget::initMoreMenu() {
  m_moreMenu->clear();

  // The same exports as the List menu, so the MyAnimeList IDs are looked up here too
  m_moreMenu->addAction(tr("Export as Markdown..."), this,
                        []() { mainWindow()->ui()->actionExportListAsMarkdown->trigger(); });

  m_moreMenu->addAction(tr("Export as XML..."), this,
                        []() { mainWindow()->ui()->actionExportListAsMyAnimeListXML->trigger(); });
}

}  // namespace gui
