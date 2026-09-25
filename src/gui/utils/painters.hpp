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

#pragma once

#include <QAbstractScrollArea>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QStyleOption>

namespace anime {
struct Details;
}

namespace anime::list {
struct Entry;
}

namespace gui {

constexpr int kSpinnerIntervalMs = 40;
constexpr qreal kSpinnerDegreesPerTick = 12.0;

void paintEmptyListText(QAbstractScrollArea* widget, const QString& text);

// With `textOutside`, the numbers sit to the right of a thinner bar, as in v1's list.
void paintProgressBar(QPainter* painter, const QStyleOption& option, const anime::Details* anime,
                      const anime::list::Entry* entry, const bool textOutside = false);

// Draws `pixmap` rotated by `angle` degrees around `center`.
void paintSpinner(QPainter* painter, const QPixmap& pixmap, const QPointF& center, qreal angle);

}  // namespace gui
