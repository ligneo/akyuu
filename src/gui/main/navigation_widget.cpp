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

#include "navigation_widget.hpp"

#include <QMouseEvent>
#include <QStyle>
#include <algorithm>

#include "akyuu/session.hpp"
#include "base/string.hpp"
#include "gui/main/main_window.hpp"
#include "gui/main/navigation_item_delegate.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "sync/anilist/anilist.hpp"
#include "sync/kitsu/kitsu.hpp"
#include "sync/myanimelist/myanimelist.hpp"
#include "sync/queue.hpp"

namespace gui {

namespace {

constexpr int kWidth = 200;
// How much wider than its labels the sidebar can be dragged
constexpr int kMaxWidthFactor = 2;

}  // namespace

NavigationWidget::NavigationWidget(QWidget* parent) : QTreeWidget(parent) {
  setObjectName("navigation");
  // Tiling compositors can make the window narrower than its minimum size. A fixed width would
  // then overlap the page, so the sidebar gives up its width once the page cannot shrink further.
  // It has a maximum width instead, which leaves room to drag it wider than its labels.
  setFrameShape(QFrame::Shape::NoFrame);
  setItemDelegate(new NavigationItemDelegate(this));
  setHeaderHidden(true);
  setIndentation(0);

  viewport()->setMouseTracking(true);

  refresh();

  connect(this, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current) {
    if (!current) return;

    constexpr auto pageIndexRole = static_cast<int>(NavigationItemDataRole::PageIndex);
    const auto page = current->data(0, pageIndexRole).value<MainWindowPage>();
    emit currentPageChanged(page);

    if (page != MainWindowPage::List) return;

    constexpr auto statusRole = static_cast<int>(NavigationItemDataRole::ListStatus);
    const auto status = current->data(0, statusRole).value<anime::list::Status>();
    emit currentListStatusChanged(status);
  });

  connect(&services::queue, &services::Queue::changed, this, &NavigationWidget::refresh);

  const QList<services::Service*> services{
      services::anilist::Service::instance(),
      services::kitsu::Service::instance(),
      services::myanimelist::Service::instance(),
  };
  for (auto* service : services) {
    connect(service, &services::Service::listEntriesFetched, this, &NavigationWidget::refresh);
  }
}

void NavigationWidget::refresh() {
  setUpdatesEnabled(false);
  clear();

  // v1's order: what is playing, the list and what is made of it, then everything that looks
  // further afield.
  addItem("Now Playing", "play_arrow", MainWindowPage::Home);
  addSeparator();

  auto listItem = addItem("Anime List", "list_alt", MainWindowPage::List);
  listItem->setExpanded(true);

  const auto statusCounts = []() {
    QMap<anime::list::Status, int> statuses;
    for (const auto& entry : anime::db.entries()) {
      statuses[entry.status] += 1;
    }
    return statuses;
  }();
  for (const auto status : anime::list::kStatuses) {
    auto item = addChildItem(listItem, formatListStatus(status));
    setItemData(item, NavigationItemDataRole::PageIndex, static_cast<int>(MainWindowPage::List));
    setItemData(item, NavigationItemDataRole::IsLastChild,
                status == anime::list::Status::PlanToWatch);
    setItemData(item, NavigationItemDataRole::ListStatus, static_cast<int>(status));
    setItemData(item, NavigationItemDataRole::Counter, statusCounts[status]);
    // Like v1, the statuses are tabs above the list; the items stay for navigating to them.
    item->setHidden(true);
  }

  auto historyItem = addItem("History", "history", MainWindowPage::History);
  setItemData(historyItem, NavigationItemDataRole::Counter, services::queue.count());
  addItem("Statistics", "bar_chart", MainWindowPage::Profile);

  addSeparator();
  addItem("Search", "search", MainWindowPage::Search);
  addItem("Seasons", "calendar_month", MainWindowPage::Seasons);
  addItem("Torrents", "rss_feed", MainWindowPage::Torrents);
  addItem("Library", "folder", MainWindowPage::Library);

  setUpdatesEnabled(true);
  updateMaximumWidth();

  emit refreshed();
}

// The splitter lays the sidebar out by its size hint, which would leave a gap next to it
QSize NavigationWidget::sizeHint() const {
  const int width = akyuu::session.sidebarWidth() > 0 ? akyuu::session.sidebarWidth()
                                                       : naturalWidth();
  return {std::min(width, maximumWidth()), QTreeWidget::sizeHint().height()};
}

// Wide enough for the longest label with its icon and counter, which a larger font or another
// language can push past the 200 pixels of v1.
int NavigationWidget::naturalWidth() const {
  int textWidth = 0;
  for (QTreeWidgetItemIterator it(const_cast<NavigationWidget*>(this)); *it; ++it) {
    if ((*it)->isHidden()) continue;
    textWidth = std::max(textWidth, QFontMetrics((*it)->font(0)).horizontalAdvance((*it)->text(0)));
  }
  const int icon = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
  const int counter = fontMetrics().horizontalAdvance(u"0000"_s);
  return std::max(kWidth, textWidth + icon + counter + 48);
}

void NavigationWidget::updateMaximumWidth() {
  setMaximumWidth(naturalWidth() * kMaxWidthFactor);
}

void NavigationWidget::changeEvent(QEvent* event) {
  if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
    updateMaximumWidth();
  }
  QTreeWidget::changeEvent(event);
}

void NavigationWidget::mouseMoveEvent(QMouseEvent* event) {
  auto cursor = Qt::CursorShape::ArrowCursor;

  if (const auto item = itemAt(event->pos())) {
    const int role = static_cast<int>(NavigationItemDataRole::IsSeparator);
    const bool isSeparator = item->data(0, role).toBool();
    if (!isSeparator && !item->isDisabled()) cursor = Qt::CursorShape::PointingHandCursor;
  }

  setCursor(cursor);

  // Dragging with a button held would move the selection, and with it the page, to every item on
  // the way. Pages change on a click instead, as in other sidebars.
  if (event->buttons() != Qt::NoButton) {
    event->accept();
    return;
  }

  QTreeView::mouseMoveEvent(event);
}

QTreeWidgetItem* NavigationWidget::addItem(const QString& text, const QString& icon,
                                           MainWindowPage page) {
  auto item = new QTreeWidgetItem(this);

  item->setFont(0, [item]() {
    auto font = item->font(0);
    font.setPointSize(10);
    font.setWeight(QFont::Weight::DemiBold);
    return font;
  }());

  item->setIcon(0, theme.getIcon(icon));
  item->setSizeHint(0, QSize{0, 32});
  item->setText(0, text);

  setItemData(item, NavigationItemDataRole::PageIndex, static_cast<int>(page));

  return item;
}

QTreeWidgetItem* NavigationWidget::addChildItem(QTreeWidgetItem* parent, const QString& text) {
  auto item = new QTreeWidgetItem(parent);

  item->setIcon(0, theme.getIcon("empty"));  // for indentation
  item->setSizeHint(0, QSize{0, 32});
  item->setText(0, text);

  setItemData(item, NavigationItemDataRole::IsChild, true);

  return item;
}

void NavigationWidget::addSeparator() {
  auto item = new QTreeWidgetItem(this);

  item->setDisabled(true);
  item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
  item->setSizeHint(0, QSize{0, 16});

  setItemData(item, NavigationItemDataRole::IsSeparator, true);
}

void NavigationWidget::setItemData(QTreeWidgetItem* item, NavigationItemDataRole role,
                                   const QVariant& value) {
  item->setData(0, static_cast<int>(role), value);
}

QTreeWidgetItem* NavigationWidget::findListStatusItem(anime::list::Status status) const {
  const auto listItem = findItemByPage(MainWindowPage::List);
  if (!listItem) return nullptr;

  constexpr auto statusRole = static_cast<int>(NavigationItemDataRole::ListStatus);

  for (int i = 0; i < listItem->childCount(); ++i) {
    const auto child = listItem->child(i);
    if (child->data(0, statusRole).value<anime::list::Status>() == status) {
      return child;
    }
  }

  return nullptr;
}

QTreeWidgetItem* NavigationWidget::findItemByPage(MainWindowPage page) const {
  const auto find = [page](this auto const& find, QTreeWidgetItem* item) -> QTreeWidgetItem* {
    const auto role = static_cast<int>(NavigationItemDataRole::PageIndex);
    const int data = item->data(0, role).toInt();
    if (data == static_cast<int>(page)) return item;
    for (int i = 0; i < item->childCount(); ++i) {
      if (const auto child = find(item->child(i))) return child;
    }
    return nullptr;
  };

  for (int i = 0; i < topLevelItemCount(); ++i) {
    if (const auto item = find(topLevelItem(i))) return item;
  }

  return nullptr;
}

}  // namespace gui
