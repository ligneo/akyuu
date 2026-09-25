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

#include "theme.hpp"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>

#include "akyuu/settings.hpp"
#include "base/file.hpp"
#include "base/string.hpp"
#include "gui/utils/svg_icon_engine.hpp"

namespace gui {

Theme::Theme() : QObject() {}

const QIcon& Theme::getIcon(const QString& key, const QString& extension, bool useSvgIconEngine) {
  if (!m_icons.contains(key)) {
    if (extension == "svg" && useSvgIconEngine) {
      m_icons[key] = QIcon(new SvgIconEngine(key));
    } else {
      m_icons[key] = QIcon(u":/icons/%1.%2"_s.arg(key, extension));
    }
  }

  return m_icons[key];
}

void Theme::applyStyle() {
  qApp->styleHints()->setColorScheme(akyuu::settings.appColorScheme());

  // Remember the platform's style, so that it can be restored later on.
  if (m_systemStyle.isEmpty()) m_systemStyle = qApp->style()->name();

  auto style = QString::fromStdString(akyuu::settings.appStyle());
  if (style.compare(akyuu::Settings::kAppStyleSystem, Qt::CaseInsensitive) == 0) {
    style = m_systemStyle;
  }
  if (qApp->style()->name().compare(style, Qt::CaseInsensitive) != 0) {
    qApp->setStyle(style);
  }

  // Our stylesheets are written for Fusion, other styles look better without them.
  if (style.compare("fusion", Qt::CaseInsensitive) == 0) {
    const QString mainStylesheet = readStylesheet("main");
    const QString themeStylesheet = readStylesheet(isDark() ? "dark" : "light");
    qApp->setStyleSheet(mainStylesheet + themeStylesheet);
  } else {
    qApp->setStyleSheet({});
  }
}

void Theme::initStyle() {
  applyStyle();

  connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this,
          [](Qt::ColorScheme scheme) { qApp->styleHints()->setColorScheme(scheme); });
}

bool Theme::isDark() const {
  const auto colorScheme = qApp->styleHints()->colorScheme();

  // Some platform themes (e.g. qt6ct) provide a palette without reporting a color scheme
  if (colorScheme == Qt::ColorScheme::Unknown) {
    const auto palette = qApp->palette();
    return palette.color(QPalette::WindowText).lightness() >
           palette.color(QPalette::Window).lightness();
  }

  return colorScheme == Qt::ColorScheme::Dark;
}

QString Theme::readStylesheet(const QString& name) const {
  return base::readFile(u":/styles/%1.qss"_s.arg(name));
}

QColor Theme::errorColor() {
  return QColor(0xe5, 0x39, 0x35);  // Red 600
}

QColor Theme::successColor() {
  return QColor(0x43, 0xa0, 0x47);  // Green 600
}

QColor Theme::warningColor() {
  return QColor(0xfb, 0x8c, 0x00);  // Orange 600
}

}  // namespace gui
