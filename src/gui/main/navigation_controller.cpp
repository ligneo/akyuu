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

#include "navigation_controller.hpp"

#include <QAction>
#include <QApplication>
#include <QMouseEvent>
#include <QSignalBlocker>

#include "gui/main/main_window.hpp"
#include "gui/main/navigation_widget.hpp"
#include "gui/main/status_bar_controller.hpp"
#include "ui_main_window.h"

namespace gui {

NavigationController::NavigationController(MainWindow* mainWindow)
    : QObject(mainWindow), m_mainWindow(mainWindow) {
  connect(mainWindow->navigation(), &NavigationWidget::currentPageChanged, this,
          [this](MainWindowPage page) { applyPage(page, /*addToHistory=*/true); });

  connect(mainWindow->ui()->actionBack, &QAction::triggered, this, &NavigationController::goBack);
  connect(mainWindow->ui()->actionForward, &QAction::triggered, this,
          &NavigationController::goForward);

  qApp->installEventFilter(this);
}

void NavigationController::navigateTo(MainWindowPage page, bool addToHistory) {
  const auto navigation = m_mainWindow->navigation();
  const auto item = navigation->findItemByPage(page);

  if (item && addToHistory) {
    navigation->setCurrentItem(item);
    return;  // `currentPageChanged` signal handles the rest
  }

  const QSignalBlocker blocker(navigation);
  if (item) {
    navigation->setCurrentItem(item);
  } else {
    navigation->setCurrentIndex({});  // no tree item to select (e.g. Profile)
  }
  applyPage(page, addToHistory);
}

void NavigationController::navigateToListStatus(anime::list::Status status) {
  const auto navigation = m_mainWindow->navigation();
  if (const auto item = navigation->findListStatusItem(status)) {
    navigation->setCurrentItem(item);
  }
}

// v1's Ctrl+Tab on the anime list: the next (or previous) status, wrapping around. The list page
// itself counts as the first stop, like v1's tab bar has no "all" tab but starts at the top.
void NavigationController::cycleListStatus(const int step) {
  const auto navigation = m_mainWindow->navigation();
  const auto listItem = navigation->findItemByPage(MainWindowPage::List);
  const auto current = navigation->currentItem();
  if (!listItem || !current || listItem->childCount() == 0) return;

  // Only while the anime list is showing, as in v1
  if (current != listItem && current->parent() != listItem) return;

  const int count = listItem->childCount();
  int index = current == listItem ? (step > 0 ? -1 : 0) : listItem->indexOfChild(current);
  index = ((index + step) % count + count) % count;

  navigation->setCurrentItem(listItem->child(index));
}

void NavigationController::applyPage(MainWindowPage page, bool addToHistory) {
  m_mainWindow->initPage(page);
  m_mainWindow->statusBarController()->clearMessage(StatusBarController::Source::Selection);
  m_mainWindow->ui()->stackedWidget->setCurrentIndex(static_cast<int>(page));

  if (addToHistory) recordPageHistory(page);
}

void NavigationController::goBack() {
  if (m_pageHistoryIndex <= 0) return;

  navigateTo(m_pageHistory.at(--m_pageHistoryIndex), /*addToHistory=*/false);
  updateHistoryActions();
}

void NavigationController::goForward() {
  if (m_pageHistoryIndex >= m_pageHistory.size() - 1) return;

  navigateTo(m_pageHistory.at(++m_pageHistoryIndex), /*addToHistory=*/false);
  updateHistoryActions();
}

void NavigationController::recordPageHistory(MainWindowPage page) {
  if (m_pageHistoryIndex >= 0 && m_pageHistory.at(m_pageHistoryIndex) == page) return;

  m_pageHistory.removeAll(page);
  m_pageHistory.append(page);
  m_pageHistoryIndex = m_pageHistory.size() - 1;

  updateHistoryActions();
}

void NavigationController::updateHistoryActions() {
  m_mainWindow->ui()->actionBack->setEnabled(m_pageHistoryIndex > 0);
  m_mainWindow->ui()->actionForward->setEnabled(m_pageHistoryIndex < m_pageHistory.size() - 1);
}

bool NavigationController::eventFilter(QObject* watched, QEvent* event) {
  const auto type = event->type();
  if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease) {
    return QObject::eventFilter(watched, event);
  }

  const auto mouseEvent = static_cast<QMouseEvent*>(event);
  if (mouseEvent->button() != Qt::BackButton && mouseEvent->button() != Qt::ForwardButton) {
    return QObject::eventFilter(watched, event);
  }

  const auto widget = qobject_cast<QWidget*>(watched);
  if (!widget) {
    // Allow redispatching top-level window's event to the real widget.
    return QObject::eventFilter(watched, event);
  }

  if (type == QEvent::MouseButtonRelease && widget->window() == m_mainWindow) {
    if (mouseEvent->button() == Qt::BackButton) {
      goBack();
    } else {
      goForward();
    }
  }

  // Consume so it never counts as a normal click (e.g. selecting a list item).
  return true;
}

}  // namespace gui
