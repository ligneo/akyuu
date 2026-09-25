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

#include "tray_icon.hpp"

#include <QMenu>
#include <QPainter>
#include <QSystemTrayIcon>

#include "gui/utils/theme.hpp"

namespace gui {

using namespace Qt::StringLiterals;

TrayIcon::TrayIcon(QObject* parent, QMenu* menu) {
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    return;
  }

  m_contextMenu = menu;

  m_icon = new QSystemTrayIcon(parent);
  m_icon->setContextMenu(m_contextMenu);
  m_icon->setIcon(moodIcon());
  m_icon->setToolTip("Akyuu");
  m_icon->show();

  connect(m_icon, &QSystemTrayIcon::activated, this,
          [this](QSystemTrayIcon::ActivationReason reason) {
            switch (reason) {
              case QSystemTrayIcon::ActivationReason::Trigger:
              case QSystemTrayIcon::ActivationReason::DoubleClick:
              case QSystemTrayIcon::ActivationReason::MiddleClick:
                emit activated();
                break;
            }
          });

  connect(m_icon, &QSystemTrayIcon::messageClicked, this, &TrayIcon::messageClicked);
}

void TrayIcon::showMessage(const QString& title, const QString& text) const {
  if (!m_icon) return;

  m_icon->showMessage(title, text, QSystemTrayIcon::Information);
}

bool TrayIcon::isVisible() const {
  return m_icon && m_icon->isVisible();
}

void TrayIcon::setBadge(Badge badge) {
  if (m_badge == badge) return;

  m_badge = badge;
  updateIcon();
}

void TrayIcon::setSleeping(bool sleeping) {
  if (m_sleeping == sleeping) return;

  m_sleeping = sleeping;
  updateIcon();
}

// The owl sleeps while detection is off, and smiles at an episode it recognized. The tray gets a
// simpler drawing than the window icon: the quill and the feet are lost at 16-22 px anyway.
QIcon TrayIcon::moodIcon() const {
  if (m_sleeping) return QIcon(u":/icons/tray/akyuu_sleepy.svg"_s);
  if (m_badge == Badge::Success) return QIcon(u":/icons/tray/akyuu_happy.svg"_s);
  return QIcon(u":/icons/tray/akyuu.svg"_s);
}

void TrayIcon::updateIcon() {
  if (!m_icon) return;

  const auto icon = moodIcon();

  if (m_badge == Badge::None || m_sleeping) {
    m_icon->setIcon(icon);
    return;
  }

  // An SVG icon has no sizes of its own, so the badge is drawn on each size a tray may ask for.
  QIcon badged;
  for (const int size : {16, 22, 24, 32, 48, 64}) {
    // At a device pixel ratio of 1, so the badge is drawn in the same pixels as the owl.
    QPixmap pixmap = icon.pixmap(QSize(size, size), 1.0);
    paintBadge(pixmap);
    badged.addPixmap(pixmap);
  }
  m_icon->setIcon(badged);
}

void TrayIcon::paintBadge(QPixmap& pixmap) const {
  const auto color = m_badge == Badge::Success ? Theme::successColor() : Theme::errorColor();

  const QSize pixmapSize = pixmap.size();
  const int size = pixmapSize.width() / 2;
  const QRect rect(pixmapSize.width() - size, pixmapSize.height() - size, size, size);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(color);
  painter.drawEllipse(rect);
}

}  // namespace gui
