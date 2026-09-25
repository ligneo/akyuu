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

#include "filter_dialog.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

#include "gui/torrents/filter_condition_dialog.hpp"
#include "gui/utils/format.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "media/anime_utils.hpp"
#include "track/feed_filter_manager.hpp"

namespace gui {

namespace {

using namespace Qt::StringLiterals;

}  // namespace

FilterDialog::FilterDialog(QWidget* parent, const track::Filter& filter)
    : QDialog(parent),
      m_filter(filter),
      m_editName(new QLineEdit(this)),
      m_comboAction(new QComboBox(this)),
      m_comboMatch(new QComboBox(this)),
      m_comboOption(new QComboBox(this)),
      m_labelOption(new QLabel(tr("Option:"), this)),
      m_listConditions(new QTreeWidget(this)),
      m_listAnime(new QListWidget(this)),
      m_buttonEdit(new QPushButton(tr("Edit"), this)),
      m_buttonRemove(new QPushButton(tr("Remove"), this)),
      m_buttonUp(new QPushButton(tr("Move up"), this)),
      m_buttonDown(new QPushButton(tr("Move down"), this)) {
  setWindowTitle(tr("Filter"));

  const auto layout = new QVBoxLayout(this);

  // v1's filter wizard: the name, the conditions, then the options
  {
    for (const auto action :
         {track::FilterAction::Discard, track::FilterAction::Select, track::FilterAction::Prefer}) {
      m_comboAction->addItem(formatFilterAction(action), static_cast<int>(action));
    }
    for (const auto match : {track::FilterMatch::All, track::FilterMatch::Any}) {
      m_comboMatch->addItem(formatFilterMatch(match), static_cast<int>(match));
    }
    for (const auto option : {track::FilterOption::Default, track::FilterOption::Deactivate,
                              track::FilterOption::Hide}) {
      m_comboOption->addItem(formatFilterOption(option), static_cast<int>(option));
    }

    const auto group = new QGroupBox(tr("Filter name"), this);
    const auto groupLayout = new QVBoxLayout(group);
    groupLayout->addWidget(m_editName);
    layout->addWidget(group);
  }

  // Conditions
  {
    const auto group = new QGroupBox(tr("Conditions"), this);
    const auto groupLayout = new QVBoxLayout(group);

    m_listConditions->setColumnCount(3);
    m_listConditions->setHeaderLabels({tr("Element"), tr("Operator"), tr("Value")});
    m_listConditions->setRootIsDecorated(false);
    m_listConditions->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    groupLayout->addWidget(m_listConditions);

    const auto buttonLayout = new QHBoxLayout();
    const auto buttonAdd = new QPushButton(tr("Add new..."), group);
    buttonLayout->addWidget(buttonAdd);
    buttonLayout->addWidget(m_buttonEdit);
    buttonLayout->addWidget(m_buttonRemove);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_buttonUp);
    buttonLayout->addWidget(m_buttonDown);
    groupLayout->addLayout(buttonLayout);

    connect(buttonAdd, &QPushButton::clicked, this, &FilterDialog::addCondition);
    connect(m_buttonEdit, &QPushButton::clicked, this, &FilterDialog::editCondition);
    connect(m_buttonRemove, &QPushButton::clicked, this, &FilterDialog::removeCondition);
    connect(m_buttonUp, &QPushButton::clicked, this, [this]() { moveCondition(-1); });
    connect(m_buttonDown, &QPushButton::clicked, this, [this]() { moveCondition(1); });
    connect(m_listConditions, &QTreeWidget::itemSelectionChanged, this,
            &FilterDialog::refreshState);
    connect(m_listConditions, &QTreeWidget::itemDoubleClicked, this, &FilterDialog::editCondition);

    layout->addWidget(group);
  }

  // Options
  {
    const auto group = new QGroupBox(tr("Options"), this);
    const auto grid = new QGridLayout(group);
    grid->addWidget(new QLabel(tr("Match:"), group), 0, 0);
    grid->addWidget(m_comboMatch, 1, 0);
    grid->addWidget(new QLabel(tr("Action:"), group), 0, 1);
    grid->addWidget(m_comboAction, 1, 1);
    grid->addWidget(m_labelOption, 0, 2);
    grid->addWidget(m_comboOption, 1, 2);
    layout->addWidget(group);
  }

  // Anime the filter is limited to
  {
    const auto group = new QGroupBox(tr("Limit this filter to certain anime"), this);
    const auto groupLayout = new QVBoxLayout(group);

    groupLayout->addWidget(
        new QLabel(tr("Leave everything unchecked to apply the filter to all anime."), group));

    QList<QPair<QString, int>> titles;
    for (const auto& [id, item] : anime::db.items().asKeyValueRange()) {
      const auto entry = anime::db.entry(id);
      const auto status = entry ? entry->status : anime::list::Status::NotInList;
      if (status == anime::list::Status::NotInList) continue;
      titles.append({QString::fromStdString(anime::preferredTitle(item)), id});
    }
    std::ranges::sort(titles, [](const auto& a, const auto& b) {
      return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
    });

    for (const auto& [title, id] : titles) {
      const auto item = new QListWidgetItem(title, m_listAnime);
      item->setData(Qt::UserRole, id);
      item->setCheckState(std::ranges::contains(m_filter.anime_ids, id) ? Qt::Checked
                                                                        : Qt::Unchecked);
    }

    groupLayout->addWidget(m_listAnime);
    layout->addWidget(group);
  }

  const auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  m_buttonOk = buttons->button(QDialogButtonBox::Ok);
  layout->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  // The option only says how discarded items are presented, so it does not apply to selection.
  connect(m_comboAction, &QComboBox::currentIndexChanged, this, &FilterDialog::refreshState);

  m_editName->setText(QString::fromStdString(m_filter.name));
  m_comboAction->setCurrentIndex(m_comboAction->findData(static_cast<int>(m_filter.action)));
  m_comboMatch->setCurrentIndex(m_comboMatch->findData(static_cast<int>(m_filter.match)));
  m_comboOption->setCurrentIndex(m_comboOption->findData(static_cast<int>(m_filter.option)));

  refreshConditions();
  resize(520, 620);
}

std::optional<track::Filter> FilterDialog::edit(QWidget* parent, const track::Filter& filter) {
  FilterDialog dialog(parent, filter);

  if (dialog.exec() != QDialog::Accepted) return std::nullopt;

  return dialog.filter();
}

track::Filter FilterDialog::filter() const {
  auto filter = m_filter;

  filter.name = m_editName->text().trimmed().toStdString();
  filter.action = static_cast<track::FilterAction>(m_comboAction->currentData().toInt());
  filter.match = static_cast<track::FilterMatch>(m_comboMatch->currentData().toInt());
  filter.option = static_cast<track::FilterOption>(m_comboOption->currentData().toInt());

  filter.anime_ids.clear();
  for (int i = 0; i < m_listAnime->count(); ++i) {
    const auto item = m_listAnime->item(i);
    if (item->checkState() == Qt::Checked)
      filter.anime_ids.push_back(item->data(Qt::UserRole).toInt());
  }

  if (filter.name.empty() && !filter.conditions.empty()) {
    filter.name = formatFilterCondition(filter.conditions.front()).toStdString();
  }

  return filter;
}

void FilterDialog::addCondition() {
  const auto condition = FilterConditionDialog::edit(this);

  if (!condition) return;

  m_filter.conditions.push_back(*condition);
  refreshConditions();
  setCurrentConditionRow(m_listConditions->topLevelItemCount() - 1);
}

void FilterDialog::editCondition() {
  const auto row = currentConditionRow();

  if (row < 0) return;

  const auto condition = FilterConditionDialog::edit(this, m_filter.conditions.at(row));

  if (!condition) return;

  m_filter.conditions.at(row) = *condition;
  refreshConditions();
  setCurrentConditionRow(row);
}

void FilterDialog::removeCondition() {
  const auto row = currentConditionRow();

  if (row < 0) return;

  m_filter.conditions.erase(m_filter.conditions.begin() + row);
  refreshConditions();
}

void FilterDialog::moveCondition(const int offset) {
  const auto row = currentConditionRow();
  const auto target = row + offset;

  if (row < 0 || target < 0 || target >= static_cast<int>(m_filter.conditions.size())) return;

  std::swap(m_filter.conditions.at(row), m_filter.conditions.at(target));
  refreshConditions();
  setCurrentConditionRow(target);
}

void FilterDialog::refreshConditions() {
  const auto row = currentConditionRow();

  m_listConditions->clear();

  for (const auto& condition : m_filter.conditions) {
    new QTreeWidgetItem(m_listConditions,
                        {formatFilterElement(condition.element), formatFilterOperator(condition.op),
                         formatFilterValue(condition)});
  }

  setCurrentConditionRow(std::min(row < 0 ? 0 : row, m_listConditions->topLevelItemCount() - 1));
  refreshState();
}

void FilterDialog::refreshState() {
  const auto row = currentConditionRow();
  const auto count = m_listConditions->topLevelItemCount();

  m_buttonEdit->setEnabled(row > -1);
  m_buttonRemove->setEnabled(row > -1);
  m_buttonUp->setEnabled(row > 0);
  m_buttonDown->setEnabled(row > -1 && row < count - 1);

  // A filter with no conditions matches everything, which would discard the whole feed. v1 says
  // as much and refuses to go on; here the button stays out of reach until there is one.
  m_buttonOk->setEnabled(count > 0);
  m_buttonOk->setToolTip(count > 0 ? QString{} : tr("A filter needs at least one condition."));

  const auto action = static_cast<track::FilterAction>(m_comboAction->currentData().toInt());
  const auto discards = action != track::FilterAction::Select;
  m_labelOption->setEnabled(discards);
  m_comboOption->setEnabled(discards);
}

std::optional<track::Filter> chooseFilterPreset(QWidget* parent) {
  QDialog dialog(parent);
  dialog.setWindowTitle(QCoreApplication::translate("gui", "New Filter"));

  const auto layout = new QVBoxLayout(&dialog);

  layout->addWidget(
      new QLabel(QCoreApplication::translate(
                     "gui", "Choose one of the preset filters, or create a custom one:"),
                 &dialog));

  const auto tree = new QTreeWidget(&dialog);
  tree->setRootIsDecorated(false);
  tree->setAllColumnsShowFocus(true);
  tree->setHeaderLabels({QCoreApplication::translate("gui", "Name"),
                         QCoreApplication::translate("gui", "Description")});

  for (const auto& preset : track::filterManager.presets()) {
    const auto item = new QTreeWidgetItem(tree);
    item->setText(0, QString::fromStdString(preset.filter.name));
    item->setText(1, QString::fromStdString(preset.description));
  }

  tree->setCurrentItem(tree->topLevelItem(0));
  tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  layout->addWidget(tree);

  const auto buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  layout->addWidget(buttons);

  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  QObject::connect(tree, &QTreeWidget::itemDoubleClicked, &dialog, &QDialog::accept);

  dialog.resize(860, 400);

  if (dialog.exec() != QDialog::Accepted) return std::nullopt;

  const auto index = tree->indexOfTopLevelItem(tree->currentItem());

  if (index < 0) return std::nullopt;

  return track::filterManager.presets().at(index).filter;
}

int FilterDialog::currentConditionRow() const {
  return m_listConditions->indexOfTopLevelItem(m_listConditions->currentItem());
}

void FilterDialog::setCurrentConditionRow(const int row) {
  m_listConditions->setCurrentItem(m_listConditions->topLevelItem(row));
}

}  // namespace gui
