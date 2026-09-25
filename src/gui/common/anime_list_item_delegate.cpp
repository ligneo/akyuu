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

#include "anime_list_item_delegate.hpp"

#include <QComboBox>
#include <QPainter>

#include "gui/models/anime_list_model.hpp"
#include "gui/utils/painter_state_saver.hpp"
#include "gui/utils/painters.hpp"
#include "gui/utils/rating.hpp"
#include "gui/utils/theme.hpp"
#include "media/anime.hpp"
#include "media/anime_list.hpp"

namespace gui {

namespace {

// The height of the band drawn above the first item of a group.
constexpr int kGroupHeaderHeight = 28;

QString groupName(const QModelIndex& index) {
  return index.data(static_cast<int>(AnimeListItemDataRole::GroupName)).toString();
}

// A header belongs to the first row of each group, so the row above decides.
bool startsGroup(const QModelIndex& index) {
  const auto name = groupName(index);
  if (name.isEmpty()) return false;
  if (index.row() == 0) return true;
  return groupName(index.siblingAtRow(index.row() - 1)) != name;
}

void paintGroupHeader(QPainter* painter, const QStyleOptionViewItem& option,
                      const QModelIndex& index) {
  const PainterStateSaver painterStateSaver(painter);

  QRect rect = option.rect;
  rect.setHeight(kGroupHeaderHeight);

  painter->setPen(Qt::NoPen);
  painter->setBrush(theme.isDark() ? QColor{255, 255, 255, 12} : QColor{0, 0, 0, 12});
  painter->drawRect(rect);

  // Only the first column carries the text; the rest of the columns just continue the band.
  if (index.column() > 0) return;

  QFont font = option.font;
  font.setBold(true);
  painter->setFont(font);
  painter->setPen(option.palette.color(QPalette::ColorRole::Text));
  painter->drawText(rect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, groupName(index));
}

}  // namespace

ListItemDelegate::ListItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void ListItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  const auto entry =
      index.data(static_cast<int>(AnimeListItemDataRole::ListEntry)).value<const ListEntry*>();
  if (!entry) return;
  auto* combobox = static_cast<QComboBox*>(editor);
  setRatingComboBoxValue(combobox, entry->score);
}

QWidget* ListItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&,
                                        const QModelIndex& index) const {
  if (index.column() != AnimeListModel::COLUMN_SCORE) return nullptr;

  auto* editor = new QComboBox(parent);
  populateRatingComboBox(editor);
  for (int i = 0; i < editor->count(); ++i) {
    editor->setItemData(i, Qt::AlignCenter, Qt::TextAlignmentRole);
  }

  return editor;
}

void ListItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                    const QModelIndex& index) const {
  const auto* combobox = static_cast<QComboBox*>(editor);
  model->setData(index, combobox->currentData(), Qt::EditRole);
}

void ListItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                            const QModelIndex&) const {
  editor->setGeometry(option.rect);
}

void ListItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const {
  QStyleOptionViewItem opt = option;

  // The first item of a group gets a taller row (see sizeHint) and gives the extra band to the
  // group header; what is left is painted as an ordinary item.
  if (startsGroup(index)) {
    paintGroupHeader(painter, opt, index);
    opt.rect.adjust(0, kGroupHeaderHeight, 0, 0);
  }

  // Grid lines
  if (index.column() > 0) {
    const PainterStateSaver painterStateSaver(painter);
    painter->setPen(theme.isDark() ? QColor{255, 255, 255, 8} : QColor{0, 0, 0, 8});
    painter->drawLine(opt.rect.topLeft(), opt.rect.bottomLeft());
  }

  switch (index.column()) {
    case AnimeListModel::COLUMN_PROGRESS: {
      const PainterStateSaver painterStateSaver(painter);
      QStyledItemDelegate::paint(painter, opt, index);
      const auto anime =
          index.data(static_cast<int>(AnimeListItemDataRole::Anime)).value<const Anime*>();
      const auto entry =
          index.data(static_cast<int>(AnimeListItemDataRole::ListEntry)).value<const ListEntry*>();
      QStyleOptionViewItem progressOption = opt;
      progressOption.rect.adjust(2, 2, -2, -2);
      paintProgressBar(painter, progressOption, anime, entry, /*textOutside=*/true);
      return;
    }
  }

  QStyledItemDelegate::paint(painter, opt, index);
}

QSize ListItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
  if (index.isValid()) {
    return QSize(0, startsGroup(index) ? 24 + kGroupHeaderHeight : 24);
  }

  return QStyledItemDelegate::sizeHint(option, index);
}

}  // namespace gui
