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

#include "painters.hpp"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>

#include "akyuu/settings.hpp"
#include "base/string.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/painter_state_saver.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime.hpp"
#include "media/anime_list.hpp"
#include "media/anime_list_utils.hpp"
#include "media/anime_utils.hpp"
#include "track/library.hpp"

namespace gui {

void paintEmptyListText(QAbstractScrollArea* area, const QString& text) {
  QPainter painter(area->viewport());

  painter.setFont([&painter]() {
    auto font = painter.font();
    font.setItalic(true);
    return font;
  }());

  painter.drawText(area->viewport()->rect(), Qt::AlignCenter, text);
}

void paintProgressBar(QPainter* painter, const QStyleOption& option, const Anime* anime,
                      const ListEntry* entry, const bool textOutside) {
  if (!anime || !entry) return;

  const int episodes = anime->episode_count;
  const int watched = std::clamp(entry->watched_episodes, 0,
                                 episodes > 0 ? episodes : std::numeric_limits<int>::max());
  const auto text = u"%1/%2"_s.arg(watched).arg(formatNumber(episodes, "?"));

  auto barRect = option.rect;
  if (textOutside) {
    const auto textWidth = option.fontMetrics.horizontalAdvance(u"000/000"_s) + 8;
    auto textRect = option.rect;
    textRect.setLeft(option.rect.right() - textWidth);
    painter->setPen(option.palette.color(QPalette::ColorRole::Text));
    painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, text);
    barRect.setRight(textRect.left() - 4);
    const auto height = std::max(8, option.rect.height() * 3 / 5);
    barRect.setTop(option.rect.center().y() - height / 2);
    barRect.setHeight(height);
  }

  // A rounded track rather than the style's square boxes, with the watched part, the bands and the
  // text kept to the same shape
  const qreal radius = theme.cornerRadius(std::min(6.0, barRect.height() / 2.0));
  QPainterPath track;
  track.addRoundedRect(QRectF{barRect}, radius, radius);

  const PainterStateSaver painterStateSaver(painter);
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setClipPath(track, Qt::IntersectClip);
  painter->fillRect(barRect, theme.isDark() ? QColor{255, 255, 255, 20} : QColor{0, 0, 0, 20});

  QRectF watchedRect{barRect};
  watchedRect.setWidth(barRect.width() * anime::list::getProgressRatio(anime, entry));
  painter->fillRect(watchedRect,
                    theme.isDark() ? QColor{12, 164, 12, 160} : QColor{12, 164, 12, 255});

  // v1 draws two more bands over the bar: the episodes that have aired and the ones already on
  // disk. Both are faint, so the watched part and the text stay readable.
  if (episodes > 0) {
    const auto band = [&](const int from, const int to, const QColor& color) {
      if (to <= from) return;
      const auto width = static_cast<double>(barRect.width()) / episodes;
      QRectF rect{barRect};
      rect.setLeft(barRect.left() + width * from);
      // In a long series an episode is narrower than a pixel, and would otherwise not be drawn.
      rect.setWidth(std::max(1.0, width * (to - from)));
      painter->fillRect(rect, color);
    };

    if (akyuu::settings.listShowAvailableEpisodes()) {
      // Each run is one rectangle, because the color is translucent and overlaps would show.
      const auto ranges = track::library()->availableEpisodeRanges(anime->id);
      for (const auto& [from, to] : track::progressBands(ranges, watched, episodes)) {
        band(from, to, QColor{12, 164, 12, 64});
      }
    }

    if (akyuu::settings.listShowAiredEpisodes()) {
      const auto aired = std::min(anime::estimateLastAiredEpisodeNumber(*anime), episodes);
      band(watched, aired, QColor{190, 190, 190, 40});
    }
  }

  if (textOutside) return;

  // White over the watched part and the text color over the rest, so that a count that crosses
  // the end of the watched part stays readable on both sides
  const auto drawText = [&](const QRectF& clip, const QColor& color) {
    painter->save();
    painter->setClipRect(clip, Qt::IntersectClip);
    painter->setPen(color);
    painter->drawText(barRect, Qt::AlignCenter, text);
    painter->restore();
  };
  drawText(watchedRect, Qt::white);
  QRectF restRect{barRect};
  restRect.setLeft(watchedRect.right());
  drawText(restRect, option.palette.color(QPalette::ColorGroup::Active, QPalette::Text));
}

void paintSpinner(QPainter* painter, const QPixmap& pixmap, const QPointF& center, qreal angle) {
  const qreal dpr = pixmap.devicePixelRatio();
  const QPointF halfSize(pixmap.width() / dpr / 2.0, pixmap.height() / dpr / 2.0);

  painter->save();
  painter->setRenderHint(QPainter::SmoothPixmapTransform);
  painter->translate(center);
  painter->rotate(angle);
  painter->drawPixmap(-halfSize, pixmap);
  painter->restore();
}

}  // namespace gui
