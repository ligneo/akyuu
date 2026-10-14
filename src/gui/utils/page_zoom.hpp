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

#pragma once

#include <QObject>
#include <QString>

class QWidget;

namespace gui {

// The zoom of a page, as in KDE applications. Each page keeps its own level.
//
// It only changes the font size of the page, with a style sheet: the application style sheet
// keeps `setFont` from reaching child widgets, but style sheets cascade. Sizes that are not text
// follow the font through `zoomScale`, the way Kirigami derives its units from the font, so that
// posters, cards and bars grow with the text.
class PageZoom final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(PageZoom)

public:
  PageZoom(QWidget* page, const QString& name);
  ~PageZoom() override = default;

  // The zoom of the page that contains `widget`, if that page has one
  static PageZoom* of(const QWidget* widget);

  void zoomIn();
  void zoomOut();
  void resetZoom();

  // Takes the text down from the zoom, in percent, for a page that shrinks to fit the window.
  // Unlike the zoom it is not kept, and it does not emit `changed`.
  void setFit(int fit);

signals:
  void changed();

private:
  void apply();
  void setZoom(int zoom);

  QWidget* m_page = nullptr;
  QString m_name;
  int m_zoom = 100;
  int m_fit = 100;
};

// How much larger than the application font the text of `widget` is, 1 without a widget
double zoomScale(const QWidget* widget);

// `size` in pixels at 100%, at the zoom of `widget`
int zoomed(int size, const QWidget* widget);

}  // namespace gui
