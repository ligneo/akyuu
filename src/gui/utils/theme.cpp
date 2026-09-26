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

namespace {

// The owl on a night sky: deep purple surfaces, the owl's purple for selections, lavender for
// links and amber, its eyes, for accents. See also `styles/akyuu.qss`.
QPalette akyuuPalette() {
  const QColor night{0x1b, 0x17, 0x26};
  const QColor deep{0x15, 0x12, 0x1f};
  const QColor raised{0x26, 0x20, 0x36};
  const QColor text{0xec, 0xe8, 0xf5};
  const QColor muted{0x8f, 0x86, 0xa8};
  const QColor purple{0x7a, 0x5c, 0xc4};
  const QColor lavender{0xc9, 0xb8, 0xf0};
  const QColor amber{0xf4, 0xb7, 0x3a};

  QPalette palette;
  palette.setColor(QPalette::Window, night);
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Base, deep);
  palette.setColor(QPalette::AlternateBase, QColor{0x1e, 0x1a, 0x2b});
  palette.setColor(QPalette::Text, text);
  palette.setColor(QPalette::PlaceholderText, muted);
  palette.setColor(QPalette::Button, raised);
  palette.setColor(QPalette::ButtonText, text);
  palette.setColor(QPalette::BrightText, amber);
  palette.setColor(QPalette::ToolTipBase, QColor{0x2a, 0x22, 0x40});
  palette.setColor(QPalette::ToolTipText, text);
  palette.setColor(QPalette::Highlight, purple);
  palette.setColor(QPalette::HighlightedText, Qt::white);
  palette.setColor(QPalette::Link, lavender);
  palette.setColor(QPalette::LinkVisited, amber);
  palette.setColor(QPalette::Accent, purple);
  palette.setColor(QPalette::Light, QColor{0x3a, 0x31, 0x50});
  palette.setColor(QPalette::Midlight, QColor{0x30, 0x28, 0x44});
  palette.setColor(QPalette::Mid, QColor{0x22, 0x1c, 0x30});
  palette.setColor(QPalette::Dark, QColor{0x10, 0x0d, 0x17});
  palette.setColor(QPalette::Shadow, Qt::black);

  for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
    palette.setColor(QPalette::Disabled, role, QColor{0x6b, 0x63, 0x80});
  }
  palette.setColor(QPalette::Disabled, QPalette::Highlight, raised);

  return palette;
}

}  // namespace

bool Theme::isAkyuuStyle() const {
  return QString::fromStdString(akyuu::settings.appStyle())
             .compare(akyuu::Settings::kAppStyleAkyuu, Qt::CaseInsensitive) == 0;
}

void Theme::applyStyle() {
  // Remember the platform's style and palette, so that they can be restored later on.
  if (m_systemStyle.isEmpty()) m_systemStyle = qApp->style()->name();
  if (!m_hasSystemPalette) {
    m_systemPalette = qApp->palette();
    m_hasSystemPalette = true;
  }

  const bool akyuuStyle = isAkyuuStyle();

  // Akyuu's style is always dark; the color scheme setting applies to the others.
  qApp->styleHints()->setColorScheme(akyuuStyle ? Qt::ColorScheme::Dark
                                                : akyuu::settings.appColorScheme());

  auto style = QString::fromStdString(akyuu::settings.appStyle());
  if (style.compare(akyuu::Settings::kAppStyleSystem, Qt::CaseInsensitive) == 0) {
    style = m_systemStyle;
  } else if (akyuuStyle) {
    style = u"fusion"_s;
  }
  if (qApp->style()->name().compare(style, Qt::CaseInsensitive) != 0) {
    qApp->setStyle(style);
  }

  qApp->setPalette(akyuuStyle ? akyuuPalette() : m_systemPalette);

  // Our stylesheets are written for Fusion, other styles look better without them.
  if (style.compare("fusion", Qt::CaseInsensitive) == 0) {
    const QString mainStylesheet = readStylesheet("main");
    const QString themeStylesheet = readStylesheet(akyuuStyle ? "akyuu"
                                                   : isDark() ? "dark"
                                                              : "light");
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
  if (isAkyuuStyle()) return true;

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
