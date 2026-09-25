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

#include "filter_condition_dialog.hpp"

#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>
#include <algorithm>
#include <vector>

#include "base/string.hpp"
#include "gui/utils/format.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "media/anime_utils.hpp"
#include "track/feed.hpp"

namespace gui {

namespace {

using namespace Qt::StringLiterals;
using track::FilterElement;
using track::FilterOperator;

// The order elements are listed in, which is v1's.
constexpr std::array kElements{
    FilterElement::MetaId,
    FilterElement::MetaStatus,
    FilterElement::MetaType,
    FilterElement::MetaEpisodes,
    FilterElement::MetaDateStart,
    FilterElement::MetaDateEnd,
    FilterElement::UserNotes,
    FilterElement::UserStatus,
    FilterElement::LocalEpisodeAvailable,
    FilterElement::EpisodeTitle,
    FilterElement::EpisodeNumber,
    FilterElement::EpisodeVersion,
    FilterElement::EpisodeGroup,
    FilterElement::EpisodeVideoResolution,
    FilterElement::EpisodeVideoType,
    FilterElement::FileTitle,
    FilterElement::FileCategory,
    FilterElement::FileDescription,
    FilterElement::FileLink,
    FilterElement::FileSize,
};

// Comparing a category with "begins with" makes no sense, so each element offers only the
// operators that suit it. This is v1's `ChooseElement()`.
std::vector<FilterOperator> operatorsFor(const FilterElement element) {
  static const std::vector<FilterOperator> comparison{
      FilterOperator::Equals,        FilterOperator::NotEquals,
      FilterOperator::IsGreaterThan, FilterOperator::IsGreaterThanOrEqualTo,
      FilterOperator::IsLessThan,    FilterOperator::IsLessThanOrEqualTo,
  };
  static const std::vector<FilterOperator> equality{
      FilterOperator::Equals,
      FilterOperator::NotEquals,
  };
  static const std::vector<FilterOperator> text{
      FilterOperator::Equals,   FilterOperator::NotEquals, FilterOperator::BeginsWith,
      FilterOperator::EndsWith, FilterOperator::Contains,  FilterOperator::NotContains,
  };

  switch (element) {
    case FilterElement::FileSize:
    case FilterElement::MetaId:
    case FilterElement::EpisodeNumber:
    case FilterElement::MetaDateStart:
    case FilterElement::MetaDateEnd:
    case FilterElement::MetaEpisodes:
      return comparison;

    case FilterElement::FileCategory:
    case FilterElement::LocalEpisodeAvailable:
    case FilterElement::MetaStatus:
    case FilterElement::MetaType:
    case FilterElement::UserStatus:
      return equality;

    case FilterElement::UserNotes:
    case FilterElement::EpisodeTitle:
    case FilterElement::EpisodeGroup:
    case FilterElement::EpisodeVideoType:
    case FilterElement::FileTitle:
    case FilterElement::FileDescription:
    case FilterElement::FileLink:
      return text;

    default: {
      static const std::vector<FilterOperator> all{
          FilterOperator::Equals,        FilterOperator::NotEquals,
          FilterOperator::IsGreaterThan, FilterOperator::IsGreaterThanOrEqualTo,
          FilterOperator::IsLessThan,    FilterOperator::IsLessThanOrEqualTo,
          FilterOperator::BeginsWith,    FilterOperator::EndsWith,
          FilterOperator::Contains,      FilterOperator::NotContains,
      };
      return all;
    }
  }
}

// Anime the user is not following are left out, as in v1: a filter about a completed or dropped
// series is not what the list is for.
QList<QPair<QString, int>> relevantAnime() {
  QList<QPair<QString, int>> list;

  for (const auto& [id, item] : anime::db.items().asKeyValueRange()) {
    const auto entry = anime::db.entry(id);
    const auto status = entry ? entry->status : anime::list::Status::NotInList;

    switch (status) {
      case anime::list::Status::NotInList:
      case anime::list::Status::Completed:
      case anime::list::Status::Dropped:
        continue;
      default:
        break;
    }

    list.append({QString::fromStdString(anime::preferredTitle(item)), id});
  }

  std::ranges::sort(list, [](const auto& a, const auto& b) {
    return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
  });

  return list;
}

}  // namespace

FilterConditionDialog::FilterConditionDialog(QWidget* parent,
                                             const track::FilterCondition& condition)
    : QDialog(parent),
      m_comboElement(new QComboBox(this)),
      m_comboOperator(new QComboBox(this)),
      m_comboValue(new QComboBox(this)) {
  setWindowTitle(tr("Condition"));

  const auto layout = new QVBoxLayout(this);
  const auto form = new QFormLayout();

  for (const auto element : kElements) {
    m_comboElement->addItem(formatFilterElement(element), static_cast<int>(element));
  }

  form->addRow(tr("Element:"), m_comboElement);
  form->addRow(tr("Operator:"), m_comboOperator);
  form->addRow(tr("Value:"), m_comboValue);
  layout->addLayout(form);

  const auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  layout->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_comboElement, &QComboBox::currentIndexChanged, this,
          &FilterConditionDialog::chooseElement);

  m_comboElement->setCurrentIndex(m_comboElement->findData(static_cast<int>(condition.element)));
  chooseElement();

  if (const auto index = m_comboOperator->findData(static_cast<int>(condition.op)); index > -1) {
    m_comboOperator->setCurrentIndex(index);
  }

  const auto value = QString::fromStdString(condition.value);
  if (const auto index = m_comboValue->findData(value); index > -1) {
    m_comboValue->setCurrentIndex(index);
  } else if (m_comboValue->isEditable()) {
    m_comboValue->setCurrentText(value);
  }

  resize(400, sizeHint().height());
}

std::optional<track::FilterCondition> FilterConditionDialog::edit(
    QWidget* parent, const track::FilterCondition& condition) {
  FilterConditionDialog dialog(parent, condition);

  if (dialog.exec() != QDialog::Accepted) return std::nullopt;

  return dialog.condition();
}

track::FilterCondition FilterConditionDialog::condition() const {
  const auto value = m_comboValue->currentData();

  return {
      .element = static_cast<FilterElement>(m_comboElement->currentData().toInt()),
      .op = static_cast<FilterOperator>(m_comboOperator->currentData().toInt()),
      // A list of fixed choices carries the stored value in the item data, while a free-text one
      // is taken as typed.
      .value = (value.isValid() && !m_comboValue->isEditable() ? value.toString()
                                                               : m_comboValue->currentText())
                   .toStdString(),
  };
}

void FilterConditionDialog::chooseElement() {
  const auto element = static_cast<FilterElement>(m_comboElement->currentData().toInt());

  // Keep the operator if the new element also offers it
  const auto previousOperator = m_comboOperator->currentData();
  m_comboOperator->clear();
  for (const auto op : operatorsFor(element)) {
    m_comboOperator->addItem(formatFilterOperator(op), static_cast<int>(op));
  }
  if (const auto index = m_comboOperator->findData(previousOperator); index > -1) {
    m_comboOperator->setCurrentIndex(index);
  }

  m_comboValue->clear();
  m_comboValue->setEditable(true);
  m_comboValue->lineEdit()->setPlaceholderText({});

  const auto addChoices = [this](const QStringList& values) {
    for (const auto& value : values) m_comboValue->addItem(value, value);
  };

  switch (element) {
    case FilterElement::FileCategory:
      m_comboValue->setEditable(false);
      for (const auto category : {track::TorrentCategory::Anime, track::TorrentCategory::Batch,
                                  track::TorrentCategory::Other}) {
        const auto name = track::torrentCategoryName(category);
        m_comboValue->addItem(name, name);
      }
      break;

    case FilterElement::FileSize:
      addChoices({u"10 MiB"_s, u"100 MiB"_s, u"1 GiB"_s});
      break;

    case FilterElement::MetaId:
      m_comboValue->setEditable(false);
      for (const auto& [title, id] : relevantAnime()) {
        m_comboValue->addItem(title, QString::number(id));
      }
      break;

    case FilterElement::EpisodeTitle:
      for (const auto& [title, id] : relevantAnime()) {
        m_comboValue->addItem(title, title);
      }
      break;

    case FilterElement::MetaDateStart:
    case FilterElement::MetaDateEnd:
      addChoices({QDate::currentDate().toString(Qt::ISODate)});
      m_comboValue->lineEdit()->setPlaceholderText(u"YYYY-MM-DD"_s);
      break;

    case FilterElement::MetaStatus:
      m_comboValue->setEditable(false);
      for (const auto status : anime::kStatuses) {
        m_comboValue->addItem(formatStatus(status), QString::number(static_cast<int>(status)));
      }
      break;

    case FilterElement::MetaType:
      m_comboValue->setEditable(false);
      for (const auto type : anime::kTypes) {
        m_comboValue->addItem(formatType(type), QString::number(static_cast<int>(type)));
      }
      break;

    case FilterElement::UserStatus:
      m_comboValue->setEditable(false);
      m_comboValue->addItem(formatListStatus(anime::list::Status::NotInList),
                            QString::number(static_cast<int>(anime::list::Status::NotInList)));
      for (const auto status : anime::list::kStatuses) {
        m_comboValue->addItem(formatListStatus(status), QString::number(static_cast<int>(status)));
      }
      break;

    case FilterElement::EpisodeNumber:
    case FilterElement::MetaEpisodes:
      addChoices({u"%watched%"_s, u"%total%"_s});
      break;

    case FilterElement::EpisodeVersion:
      addChoices({u"2"_s, u"3"_s, u"4"_s, u"0"_s});
      break;

    case FilterElement::LocalEpisodeAvailable:
      m_comboValue->setEditable(false);
      addChoices({u"False"_s, u"True"_s});
      break;

    case FilterElement::EpisodeVideoResolution:
      addChoices({u"1080p"_s, u"720p"_s, u"480p"_s, u"400p"_s});
      break;

    case FilterElement::EpisodeVideoType:
      addChoices({u"h264"_s, u"x264"_s, u"XviD"_s});
      break;

    default:
      break;
  }

  if (m_comboValue->isEditable()) m_comboValue->setCurrentText({});
}

}  // namespace gui
