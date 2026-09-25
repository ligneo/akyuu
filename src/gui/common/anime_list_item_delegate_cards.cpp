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

#include "anime_list_item_delegate_cards.hpp"

#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <algorithm>

#include "base/string.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/painter_state_saver.hpp"
#include "gui/utils/painters.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime_season.hpp"

namespace gui {

constexpr int itemHeight = 210;
constexpr int posterWidth = itemHeight * 2 / 3;
constexpr int kSpinnerSize = 24;

ListItemDelegateCards::ListItemDelegateCards(QObject* parent) : QStyledItemDelegate(parent) {
  m_spinnerPixmap = theme.getIcon("progress_activity").pixmap(QSize(kSpinnerSize, kSpinnerSize));

  connect(&m_timerSpinner, &QTimer::timeout, this, &ListItemDelegateCards::advanceSpinner);
}

void ListItemDelegateCards::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const {
  const PainterStateSaver painterStateSaver(painter);

  const auto font = painter->font();

  const auto item =
      index.data(static_cast<int>(AnimeListItemDataRole::Anime)).value<const Anime*>();
  const auto entry =
      index.data(static_cast<int>(AnimeListItemDataRole::ListEntry)).value<const ListEntry*>();

  QStyleOptionViewItem opt = option;
  QRect rect = opt.rect;

  QPainterPath path;
  path.addRoundedRect(rect, 4, 4);
  painter->setClipPath(path);

  // Background
  if (option.state & QStyle::State_Selected) {
    painter->fillRect(rect, opt.palette.highlight());
  } else if (theme.isDark()) {
    painter->fillRect(rect, opt.palette.mid());
  } else {
    painter->fillRect(rect, opt.palette.alternateBase());
  }

  // Poster
  {
    QRect posterRect = rect;
    posterRect.setWidth(posterWidth);

    if (theme.isDark()) {
      painter->fillRect(posterRect, opt.palette.dark());
    } else {
      painter->fillRect(posterRect, opt.palette.mid());
    }

    const auto pixmap =
        index.data(static_cast<int>(AnimeListItemDataRole::Poster)).value<QPixmap>();

    if (!pixmap.isNull()) {
      m_loadingIndices.remove(index);

      const auto scaled =
          pixmap.size().scaled(posterRect.size(), Qt::AspectRatioMode::KeepAspectRatioByExpanding);

      QRect sourceRect{pixmap.rect()};
      if (scaled.width() > posterRect.width()) {
        const auto half = (scaled.width() - posterRect.width()) / 2.0f;
        const auto scale = static_cast<float>(pixmap.width()) / scaled.width();
        sourceRect.adjust(half * scale, 0, -half * scale, 0);
      } else {
        const auto half = (scaled.height() - posterRect.height()) / 2.0f;
        const auto scale = static_cast<float>(pixmap.height()) / scaled.height();
        sourceRect.adjust(0, half * scale, 0, -half * scale);
      }

      painter->drawPixmap(posterRect, pixmap, sourceRect);
    } else if (!item->image_url.empty()) {
      m_loadingIndices.insert(index);
      if (!m_timerSpinner.isActive()) {
        m_timerSpinner.start(kSpinnerIntervalMs);
      }

      paintSpinner(painter, m_spinnerPixmap, posterRect.center(), m_angle);
    } else {
      m_loadingIndices.remove(index);
    }
  }

  if (entry) {
    auto progressOptions = opt;
    progressOptions.rect = rect;
    progressOptions.rect.setWidth(posterWidth);
    progressOptions.rect.setTop(progressOptions.rect.bottom() - 28);
    progressOptions.rect.adjust(4, 4, -4, -4);
    paintProgressBar(painter, progressOptions, item, entry);
  }

  rect.adjust(posterWidth, 0, 0, 0);

  // Title
  {
    QRect titleRect = rect;
    titleRect.setHeight(32);

    painter->fillRect(titleRect, opt.palette.dark());
    titleRect.adjust(12, 0, -12, 0);

    auto titleFont = font;
    titleFont.setPointSize(10);
    titleFont.setWeight(QFont::Weight::DemiBold);
    painter->setFont(titleFont);

    const QString title = index.data(Qt::DisplayRole).toString();
    const QFontMetrics metrics(painter->font());
    const QString elidedTitle = metrics.elidedText(title, Qt::ElideRight, titleRect.width());

    painter->drawText(titleRect, Qt::AlignVCenter | Qt::TextSingleLine, elidedTitle);

    rect.adjust(12, 32 + 8, -12, -12);
  }

  // Details, one labelled row each as in v1's season tiles
  {
    auto detailsFont = font;
    detailsFont.setPointSize(9);
    painter->setFont(detailsFont);
    const QFontMetrics metrics(painter->font());

    const bool hasStudios = !item->studios.empty();
    const QList<QPair<QString, QString>> rows{
        {tr("Aired:"),
         u"%1 (%2)"_s.arg(formatFuzzyDateRange(item->date_started, item->date_finished))
             .arg(formatStatus(item->status))},
        {tr("Type:"), formatType(item->type)},
        {tr("Episodes:"), formatNumber(item->episode_count, "?")},
        {tr("Genres:"), joinStrings(item->genres)},
        {hasStudios ? tr("Studios:") : tr("Producers:"),
         joinStrings(hasStudios ? item->studios : item->producers)},
        {tr("Score:"), item->score > 0 ? formatScore(item->score) : u"?"_s},
    };

    int labelWidth = 0;
    for (const auto& row : rows) {
      labelWidth = std::max(labelWidth, metrics.horizontalAdvance(row.first));
    }
    labelWidth += 8;

    QRect rowRect = rect;
    rowRect.setHeight(metrics.height());

    for (const auto& [label, value] : rows) {
      painter->setPen(opt.palette.placeholderText().color());
      painter->drawText(rowRect, Qt::AlignVCenter | Qt::TextSingleLine, label);

      painter->setPen(opt.palette.text().color());
      QRect valueRect = rowRect.adjusted(labelWidth, 0, 0, 0);
      painter->drawText(valueRect, Qt::AlignVCenter | Qt::TextSingleLine,
                        metrics.elidedText(value, Qt::ElideRight, valueRect.width()));

      rowRect.translate(0, metrics.height());
    }

    rect.adjust(0, (metrics.height() * rows.size()) + 6, 0, 0);
  }

  // Synopsis
  {
    QString synopsis = QString::fromStdString(item->synopsis);
    synopsis.replace("<br>", "\n");
    removeHtmlTags(synopsis);
    synopsis = synopsis.simplified();

    painter->setPen(opt.palette.placeholderText().color());

    auto synopsisFont = painter->font();
    synopsisFont.setPointSize(8);
    painter->setFont(synopsisFont);
    const QFontMetrics metrics(painter->font());

    QRect synopsisRect = rect;
    synopsisRect.setHeight(qMin(synopsisRect.height(), metrics.height() * 5));

    painter->drawText(synopsisRect, Qt::TextWordWrap, synopsis);
  }
}

QSize ListItemDelegateCards::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const {
  if (index.isValid()) return itemSize();
  return QStyledItemDelegate::sizeHint(option, index);
}

void ListItemDelegateCards::initStyleOption(QStyleOptionViewItem* option,
                                            const QModelIndex& index) const {
  QStyledItemDelegate::initStyleOption(option, index);

  option->features &= ~QStyleOptionViewItem::ViewItemFeature::HasDisplay;
  option->features &= ~QStyleOptionViewItem::ViewItemFeature::HasDecoration;
}

void ListItemDelegateCards::advanceSpinner() {
  m_angle += kSpinnerDegreesPerTick;
  if (m_angle >= 360.0) m_angle -= 360.0;

  auto* view = reinterpret_cast<QListView*>(parent());

  for (auto it = m_loadingIndices.begin(); it != m_loadingIndices.end();) {
    const QModelIndex index = *it;
    const bool stillLoading =
        index.isValid() &&
        index.data(static_cast<int>(AnimeListItemDataRole::Poster)).value<QPixmap>().isNull();

    if (!stillLoading) {
      it = m_loadingIndices.erase(it);
      continue;
    }

    view->update(index);
    ++it;
  }

  if (m_loadingIndices.isEmpty()) m_timerSpinner.stop();
}

QSize ListItemDelegateCards::itemSize() const {
  constexpr int maxColumns = 4;
  constexpr int maxItemWidth = 360;

  const auto parent = reinterpret_cast<QListView*>(this->parent());
  const int spacing = parent->spacing();

  const int availableWidth =
      parent->geometry().width() - ((2 * spacing) + parent->verticalScrollBar()->width());

  const int columns = [&]() {
    for (int i = maxColumns; i >= 1; --i) {
      if (availableWidth - (i * spacing) > i * maxItemWidth) return i;
    }
    return 1;
  }();

  const int columnsWidth = availableWidth - (columns * spacing);
  const float itemWidth = columnsWidth / static_cast<float>(columns);

  return QSize(std::floor(itemWidth), itemHeight);
}

}  // namespace gui
