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

#include "anime_list_view.hpp"

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>

#include "akyuu/session.hpp"
#include "akyuu/settings.hpp"
#include "gui/common/anime_list_item_delegate.hpp"
#include "gui/common/anime_list_view_base.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/models/anime_list_proxy_model.hpp"
#include "gui/utils/painters.hpp"
#include "track/play.hpp"

namespace gui {

namespace {

QList<int> defaultHiddenColumns(const AnimeListContext context) {
  QList<int> columns{
      AnimeListModel::COLUMN_DURATION, AnimeListModel::COLUMN_REWATCHES,
      AnimeListModel::COLUMN_STARTED,  AnimeListModel::COLUMN_COMPLETED,
      AnimeListModel::COLUMN_NOTES,
  };
  if (context == AnimeListContext::Search) {
    columns.append({AnimeListModel::COLUMN_SCORE, AnimeListModel::COLUMN_LAST_UPDATED});
  } else {
    columns.append(AnimeListModel::COLUMN_AVERAGE);
  }
  return columns;
}

std::optional<QList<int>> savedHiddenColumns(const AnimeListContext context) {
  return context == AnimeListContext::Search ? akyuu::session.searchListHiddenColumns()
                                             : akyuu::session.animeListHiddenColumns();
}

void saveHiddenColumns(const AnimeListContext context, const QList<int>& columns) {
  if (context == AnimeListContext::Search) {
    akyuu::session.setSearchListHiddenColumns(columns);
  } else {
    akyuu::session.setAnimeListHiddenColumns(columns);
  }
}

}  // namespace

ListView::ListView(QWidget* parent, AnimeListModel* model, AnimeListProxyModel* proxyModel,
                   AnimeListContext context)
    : m_base(new ListViewBase(parent, this, model, proxyModel, context)) {
  setObjectName("animeList");

  setFrameShape(QFrame::Shape::NoFrame);

  setAlternatingRowColors(true);
  setItemDelegate(new ListItemDelegate(this));

  setAllColumnsShowFocus(true);
  setExpandsOnDoubleClick(false);
  setItemsExpandable(false);
  setRootIsDecorated(false);

  // Rows are all the same height until the list is grouped: then the first row of each group is
  // taller, because the delegate draws the group header in the space it gains.
  const auto applyGrouping = [this, proxyModel]() {
    setUniformRowHeights(proxyModel->groupBy() == AnimeListGroupBy::None);
    scheduleDelayedItemsLayout();
  };
  applyGrouping();
  connect(proxyModel, &AnimeListProxyModel::groupByChanged, this, applyGrouping);

  header()->setFirstSectionMovable(true);
  header()->setStretchLastSection(false);
  header()->setTextElideMode(Qt::ElideRight);
  setHiddenColumns(savedHiddenColumns(context).value_or(defaultHiddenColumns(context)));

  // v1's `AnimeListHeaders` menu: pick the columns to show, or go back to the defaults.
  header()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(header(), &QWidget::customContextMenuRequested, this, [this, context](const QPoint& pos) {
    QMenu menu(this);
    for (int column = 0; column < header()->count(); ++column) {
      const auto action = menu.addAction(
          this->model()->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString());
      action->setCheckable(true);
      action->setChecked(!header()->isSectionHidden(column));
      // The title is what the rest of the row hangs on, so it always stays
      action->setEnabled(column != AnimeListModel::COLUMN_TITLE);
      connect(action, &QAction::toggled, this, [this, context, column](const bool checked) {
        header()->setSectionHidden(column, !checked);
        saveHiddenColumns(context, hiddenColumns());
      });
    }
    menu.addSeparator();
    menu.addAction(tr("Reset to defaults"), this, [this, context]() {
      setHiddenColumns(defaultHiddenColumns(context));
      saveHiddenColumns(context, hiddenColumns());
    });
    menu.exec(header()->viewport()->mapToGlobal(pos));
  });
  header()->resizeSection(AnimeListModel::COLUMN_TITLE, 295);
  header()->resizeSection(AnimeListModel::COLUMN_PROGRESS, 150);
  header()->resizeSection(AnimeListModel::COLUMN_DURATION, 75);
  header()->resizeSection(AnimeListModel::COLUMN_SCORE, 75);
  header()->resizeSection(AnimeListModel::COLUMN_AVERAGE, 75);
  header()->resizeSection(AnimeListModel::COLUMN_TYPE, 75);
  header()->resizeSection(AnimeListModel::COLUMN_LAST_UPDATED, 110);

  // `sortByColumn` needs to be called before `setSortingEnabled`.
  // Otherwise the sort column is set to `0`.
  sortByColumn(proxyModel->sortColumn(), proxyModel->sortOrder());
  setSortingEnabled(true);

  connect(this, &QAbstractItemView::clicked, this,
          qOverload<const QModelIndex&>(&QAbstractItemView::edit));
}

void ListView::keyPressEvent(QKeyEvent* event) {
  if (m_base->handleKeyPress(event)) return;

  QTreeView::keyPressEvent(event);
}

void ListView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::MouseButton::MiddleButton) {
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
      setCurrentIndex(index);
      // Searching is about finding a title rather than watching it, so that page keeps its own
      // behavior; everywhere else the setting decides. v1 has the same split.
      if (m_base->context() == AnimeListContext::Search) {
        m_base->openAnimePage(index);
      } else {
        m_base->triggerClickAction(index, akyuu::settings.listMiddleClickAction());
      }
      return;
    }
  }

  QTreeView::mousePressEvent(event);
}

void ListView::paintEvent(QPaintEvent* event) {
  if (model() && model()->rowCount() == 0) {
    paintEmptyListText(this, tr("No items found."));
  }

  QTreeView::paintEvent(event);
}

QList<int> ListView::hiddenColumns() const {
  QList<int> columns;
  for (int column = 0; column < header()->count(); ++column) {
    if (header()->isSectionHidden(column)) columns.append(column);
  }
  return columns;
}

void ListView::setHiddenColumns(const QList<int>& columns) {
  for (int column = 0; column < header()->count(); ++column) {
    header()->setSectionHidden(column, columns.contains(column));
  }
}

}  // namespace gui
