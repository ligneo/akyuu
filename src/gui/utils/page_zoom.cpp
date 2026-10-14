/**
 * Akyuu
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

#include "page_zoom.hpp"

#include <QApplication>
#include <QWidget>
#include <algorithm>

#include "akyuu/settings.hpp"

namespace gui {

using namespace Qt::StringLiterals;

namespace {

// In percent, in steps like the zoom of a document
constexpr int kZoomMin = 70;
constexpr int kZoomMax = 200;
constexpr int kZoomStep = 10;

double fontSize(const QFont& font) {
  return font.pointSizeF() > 0 ? font.pointSizeF() : font.pixelSize();
}

}  // namespace

PageZoom::PageZoom(QWidget* page, const QString& name) : QObject(page), m_page(page), m_name(name) {
  m_zoom = std::clamp(akyuu::settings.pageZoom(m_name), kZoomMin, kZoomMax);
  if (m_zoom != 100) apply();
}

PageZoom* PageZoom::of(const QWidget* widget) {
  for (auto w = widget; w; w = w->parentWidget()) {
    if (const auto zoom = w->findChild<PageZoom*>(Qt::FindDirectChildrenOnly)) return zoom;
  }
  return nullptr;
}

void PageZoom::zoomIn() {
  setZoom(m_zoom + kZoomStep);
}

void PageZoom::zoomOut() {
  setZoom(m_zoom - kZoomStep);
}

void PageZoom::resetZoom() {
  setZoom(100);
}

void PageZoom::setZoom(const int zoom) {
  const auto clamped = std::clamp(zoom, kZoomMin, kZoomMax);
  if (clamped == m_zoom) return;
  m_zoom = clamped;
  akyuu::settings.setPageZoom(m_name, m_zoom);
  apply();
  emit changed();
}

void PageZoom::setFit(const int fit) {
  if (fit == m_fit) return;
  m_fit = fit;
  apply();
}

void PageZoom::apply() {
  // At 100% the page is left exactly as it was, without a style sheet of its own
  if (m_zoom * m_fit == 100 * 100) {
    m_page->setStyleSheet({});
  } else {
    const auto font = QApplication::font();
    const auto unit = font.pointSizeF() > 0 ? u"pt"_s : u"px"_s;
    const auto size = fontSize(font) * m_zoom / 100.0 * m_fit / 100.0;
    m_page->setStyleSheet(u"* { font-size: %1%2; }"_s.arg(size, 0, 'f', 2).arg(unit));
  }
}

double zoomScale(const QWidget* widget) {
  if (!widget) return 1.0;
  const auto base = fontSize(QApplication::font());
  return base > 0 ? fontSize(widget->font()) / base : 1.0;
}

int zoomed(const int size, const QWidget* widget) {
  return qRound(size * zoomScale(widget));
}

}  // namespace gui
