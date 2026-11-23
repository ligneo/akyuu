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

#include "anime_list_view.hpp"

#include <QDateTime>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QResizeEvent>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <algorithm>

#include "akyuu/session.hpp"
#include "akyuu/settings.hpp"
#include "gui/common/anime_list_item_delegate.hpp"
#include "gui/common/anime_list_view_base.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/models/anime_list_proxy_model.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/painters.hpp"
#include "media/anime.hpp"
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

std::optional<QList<int>> savedColumnWidths(const AnimeListContext context) {
  return context == AnimeListContext::Search ? akyuu::session.searchListColumnWidths()
                                             : akyuu::session.animeListColumnWidths();
}

void saveColumnWidths(const AnimeListContext context, const QList<int>& widths) {
  if (context == AnimeListContext::Search) {
    akyuu::session.setSearchListColumnWidths(widths);
  } else {
    akyuu::session.setAnimeListColumnWidths(widths);
  }
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
    : QTreeView(parent), m_base(new ListViewBase(this, model, proxyModel, context)) {
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
        fillWidth();
      });
    }
    menu.addSeparator();
    menu.addAction(tr("Reset to defaults"), this, [this, context]() {
      setHiddenColumns(defaultHiddenColumns(context));
      saveHiddenColumns(context, hiddenColumns());
      fillWidth();
    });
    menu.exec(header()->viewport()->mapToGlobal(pos));
  });
  header()->resizeSection(AnimeListModel::COLUMN_TITLE, 295);
  header()->resizeSection(AnimeListModel::COLUMN_PROGRESS, 220);
  header()->resizeSection(AnimeListModel::COLUMN_DURATION, 75);
  header()->resizeSection(AnimeListModel::COLUMN_SCORE, 75);
  header()->resizeSection(AnimeListModel::COLUMN_AVERAGE, 75);
  header()->resizeSection(AnimeListModel::COLUMN_TYPE, 75);
  header()->resizeSection(AnimeListModel::COLUMN_SEASON, 135);
  header()->resizeSection(AnimeListModel::COLUMN_LAST_UPDATED, 130);
  if (const auto widths = savedColumnWidths(context)) {
    for (int column = 0; column < std::min(header()->count(), int(widths->size())); ++column) {
      if (widths->at(column) > 0) header()->resizeSection(column, widths->at(column));
    }
  }
  fitColumns();

  // Like v1, the title takes whatever width the other columns leave, but it can still be dragged.
  // A dragged edge moves only between its two columns, so that the row keeps the width of the
  // list.
  connect(header(), &QHeaderView::sectionResized, this,
          [this, context](const int column, const int oldSize, const int newSize) {
            if (m_adjusting || !(QGuiApplication::mouseButtons() & Qt::LeftButton)) return;
            moveEdge(column, newSize - oldSize);
            QList<int> widths;
            for (int i = 0; i < header()->count(); ++i) widths.append(header()->sectionSize(i));
            saveColumnWidths(context, widths);
          });

  // `sortByColumn` needs to be called before `setSortingEnabled`.
  // Otherwise the sort column is set to `0`.
  sortByColumn(proxyModel->sortColumn(), proxyModel->sortOrder());
  setSortingEnabled(true);

  connect(this, &QAbstractItemView::clicked, this,
          qOverload<const QModelIndex&>(&QAbstractItemView::edit));
}

void ListView::resizeEvent(QResizeEvent* event) {
  QTreeView::resizeEvent(event);
  fillWidth();
}

void ListView::fillWidth() {
  int others = 0;
  for (int column = 0; column < header()->count(); ++column) {
    if (column == AnimeListModel::COLUMN_TITLE || header()->isSectionHidden(column)) continue;
    others += header()->sectionSize(column);
  }
  const int width = std::max(viewport()->width() - others, kMinimumTitleWidth);
  m_adjusting = true;
  header()->resizeSection(AnimeListModel::COLUMN_TITLE, width);
  m_adjusting = false;
}

// Gives the width a column gained to the next column on the right, or takes the width it lost
// from it. The last column trades with the title instead.
void ListView::moveEdge(const int column, const int delta) {
  int neighbor = -1;
  for (int visual = header()->visualIndex(column) + 1; visual < header()->count(); ++visual) {
    const int next = header()->logicalIndex(visual);
    if (!header()->isSectionHidden(next)) {
      neighbor = next;
      break;
    }
  }
  if (neighbor < 0 && column != AnimeListModel::COLUMN_TITLE) {
    neighbor = AnimeListModel::COLUMN_TITLE;
  }
  if (neighbor < 0) return;

  // A column keeps room for its name, as an unreadable header is hard to find and drag back
  const auto label = model()->headerData(neighbor, Qt::Horizontal).toString();
  const int minimum = neighbor == AnimeListModel::COLUMN_TITLE
                          ? kMinimumTitleWidth
                          : header()->fontMetrics().horizontalAdvance(label) + 32;
  const int available = header()->sectionSize(neighbor) - minimum;

  m_adjusting = true;
  if (delta > available) {
    // The neighbor cannot give more, so the dragged column stops there
    header()->resizeSection(column, header()->sectionSize(column) - delta + available);
    header()->resizeSection(neighbor, minimum);
  } else {
    header()->resizeSection(neighbor, header()->sectionSize(neighbor) - delta);
  }
  m_adjusting = false;
}

void ListView::changeEvent(QEvent* event) {
  QTreeView::changeEvent(event);
  if (event->type() == QEvent::FontChange) fitColumns();
}

// Some columns are narrow, so a larger font or zoom would cut "Special" or "11 months ago" short.
// They are widened to fit, but never narrowed, which would undo a width the user dragged them to.
void ListView::fitColumns() {
  using enum anime::Type;
  QStringList types;
  for (const auto type : {Tv, Ova, Movie, Special, Ona, Music}) {
    types.append(formatType(type));
  }
  fitColumn(AnimeListModel::COLUMN_TYPE, types);

  QStringList times;
  const auto now = QDateTime::currentSecsSinceEpoch();
  for (const qint64 ago : {44 * 60, 21 * 3600, 24 * 86400, 300 * 86400, 3650 * 86400}) {
    times.append(formatAsRelativeTime(now - ago, {}));
  }
  fitColumn(AnimeListModel::COLUMN_LAST_UPDATED, times);
}

void ListView::fitColumn(const int column, const QStringList& texts) {
  // The style measures the cell, so that the padding of the style sheet is counted as well
  QStyleOptionViewItem option;
  initViewItemOption(&option);
  option.features |= QStyleOptionViewItem::HasDisplay;

  int width = 0;
  for (const auto& text : texts) {
    option.text = text;
    width = std::max(width,
                     style()->sizeFromContents(QStyle::CT_ItemViewItem, &option, {}, this).width());
  }

  if (header()->sectionSize(column) < width) {
    header()->resizeSection(column, width);
    fillWidth();
  }
}

bool ListView::event(QEvent* event) {
  if (event->type() == QEvent::ShortcutOverride &&
      ListViewBase::keepsShortcut(static_cast<QKeyEvent*>(event))) {
    event->accept();
    return true;
  }
  return QTreeView::event(event);
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
