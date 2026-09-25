/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
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

#include "settings_torrent_filters_page.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

#include "akyuu/settings.hpp"
#include "gui/torrents/filter_dialog.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/widgets.hpp"
#include "track/feed_filter_manager.hpp"
#include "track/feed_filter_util.hpp"

namespace gui {

namespace {

using namespace Qt::StringLiterals;

// One line per filter, the way v1's list reads: what it does, then what it is made of.
QString describeFilter(const track::Filter& filter) {
  QStringList conditions;

  for (const auto& condition : filter.conditions) {
    conditions.append(formatFilterCondition(condition));
  }

  const auto separator = filter.match == track::FilterMatch::All ? u" & "_s : u" | "_s;

  return u"%1 — %2"_s.arg(formatFilterAction(filter.action)).arg(conditions.join(separator));
}

// v1 shares filters as one line of text rather than a file, so both ends of the trip are a box
// the user can copy from or paste into.
std::optional<QString> textDialog(QWidget* parent, const QString& title, const QString& info,
                                  const QString& text, const bool readOnly) {
  QDialog dialog(parent);
  dialog.setWindowTitle(title);

  const auto layout = new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel(info, &dialog));

  const auto edit = new QPlainTextEdit(text, &dialog);
  edit->setReadOnly(readOnly);
  edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  layout->addWidget(edit);

  const auto buttons = new QDialogButtonBox(
      readOnly ? QDialogButtonBox::Close : QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
      &dialog);
  layout->addWidget(buttons);

  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  dialog.resize(560, 300);

  if (readOnly) {
    edit->selectAll();
    dialog.exec();
    return std::nullopt;
  }

  if (dialog.exec() != QDialog::Accepted) return std::nullopt;

  return edit->toPlainText();
}

}  // namespace

TorrentFiltersPage::TorrentFiltersPage(QWidget* parent)
    : SettingsPage(parent),
      m_checkEnabled(new QCheckBox(tr("Enable torrent filters"), this)),
      m_listFilters(new QListWidget(this)),
      m_buttonEdit(new QPushButton(tr("Edit"), this)),
      m_buttonRemove(new QPushButton(tr("Remove"), this)),
      m_buttonUp(new QPushButton(tr("Move up"), this)),
      m_buttonDown(new QPushButton(tr("Move down"), this)) {
  const auto layout = new QVBoxLayout(this);

  layout->addWidget(m_checkEnabled);

  const auto group = new QGroupBox(tr("Filters"), this);
  const auto groupLayout = new QVBoxLayout(group);

  const auto note = new QLabel(
      tr("Filters are applied in order, and an item that has been discarded is left alone by the "
         "ones that follow."),
      group);
  note->setWordWrap(true);
  groupLayout->addWidget(note);

  groupLayout->addWidget(m_listFilters);

  const auto buttonLayout = new QHBoxLayout();
  const auto buttonAdd = new QPushButton(tr("Add new..."), group);
  buttonLayout->addWidget(buttonAdd);
  buttonLayout->addWidget(m_buttonEdit);
  buttonLayout->addWidget(m_buttonRemove);
  buttonLayout->addStretch();
  buttonLayout->addWidget(m_buttonUp);
  buttonLayout->addWidget(m_buttonDown);
  groupLayout->addLayout(buttonLayout);

  // v1 keeps these apart from the editing buttons, because they act on the whole list.
  const auto listButtonLayout = new QHBoxLayout();
  const auto buttonImport = new QPushButton(tr("Import filters..."), group);
  const auto buttonExport = new QPushButton(tr("Export filters..."), group);
  const auto buttonReset = new QPushButton(tr("Reset filters"), group);
  listButtonLayout->addWidget(buttonImport);
  listButtonLayout->addWidget(buttonExport);
  listButtonLayout->addStretch();
  listButtonLayout->addWidget(buttonReset);
  groupLayout->addLayout(listButtonLayout);

  layout->addWidget(group);

  connect(buttonAdd, &QPushButton::clicked, this, &TorrentFiltersPage::addFilter);
  connect(m_buttonEdit, &QPushButton::clicked, this, &TorrentFiltersPage::editFilter);
  connect(m_buttonRemove, &QPushButton::clicked, this, &TorrentFiltersPage::removeFilter);
  connect(m_buttonUp, &QPushButton::clicked, this, [this]() { moveFilter(-1); });
  connect(m_buttonDown, &QPushButton::clicked, this, [this]() { moveFilter(1); });
  connect(m_listFilters, &QListWidget::itemSelectionChanged, this,
          &TorrentFiltersPage::refreshState);
  connect(m_listFilters, &QListWidget::itemDoubleClicked, this, &TorrentFiltersPage::editFilter);
  connect(buttonImport, &QPushButton::clicked, this, &TorrentFiltersPage::importFilters);
  connect(buttonExport, &QPushButton::clicked, this, &TorrentFiltersPage::exportFilters);
  connect(buttonReset, &QPushButton::clicked, this, &TorrentFiltersPage::resetFilters);
  connect(m_checkEnabled, &QCheckBox::toggled, this,
          [this](bool enabled) { m_listFilters->setEnabled(enabled); });
}

void TorrentFiltersPage::load() {
  m_checkEnabled->setChecked(akyuu::settings.torrentFilterEnabled());
  m_filters = track::filterManager.filters();
  refreshList();
  m_listFilters->setEnabled(m_checkEnabled->isChecked());
}

void TorrentFiltersPage::save() {
  akyuu::settings.setTorrentFilterEnabled(m_checkEnabled->isChecked());
  applyCheckStates();

  track::filterManager.setFilters(m_filters);
}

// Only the list knows whether a filter is checked, and rebuilding it reads the flag back out of
// `m_filters`. Every edit has to take the check states along first, or it would undo them.
void TorrentFiltersPage::applyCheckStates() {
  if (m_listFilters->count() != static_cast<int>(m_filters.size())) return;

  for (int i = 0; i < m_listFilters->count(); ++i) {
    m_filters.at(i).enabled = m_listFilters->item(i)->checkState() == Qt::Checked;
  }
}

void TorrentFiltersPage::addFilter() {
  applyCheckStates();

  const auto preset = chooseFilterPreset(this);

  if (!preset) return;

  const auto filter = FilterDialog::edit(this, *preset);

  if (!filter) return;

  m_filters.push_back(*filter);
  refreshList();
  m_listFilters->setCurrentRow(m_listFilters->count() - 1);
}

void TorrentFiltersPage::editFilter() {
  applyCheckStates();

  const auto row = m_listFilters->currentRow();

  if (row < 0) return;

  const auto filter = FilterDialog::edit(this, m_filters.at(row));

  if (!filter) return;

  m_filters.at(row) = *filter;
  refreshList();
  m_listFilters->setCurrentRow(row);
}

void TorrentFiltersPage::removeFilter() {
  applyCheckStates();

  const auto row = m_listFilters->currentRow();

  if (row < 0) return;

  m_filters.erase(m_filters.begin() + row);
  refreshList();
}

void TorrentFiltersPage::moveFilter(const int offset) {
  applyCheckStates();

  const auto row = m_listFilters->currentRow();
  const auto target = row + offset;

  if (row < 0 || target < 0 || target >= static_cast<int>(m_filters.size())) return;

  std::swap(m_filters.at(row), m_filters.at(target));
  refreshList();
  m_listFilters->setCurrentRow(target);
}

void TorrentFiltersPage::importFilters() {
  const auto text =
      textDialog(this, tr("Import Filters"),
                 tr("Paste the filter text below. This replaces your current filters."), {}, false);

  if (!text || text->trimmed().isEmpty()) return;

  const auto filters = track::util::decodeFilters(*text);

  if (!filters) {
    QMessageBox::warning(this, tr("Import Filters"),
                         tr("Could not read the filter text. It may be missing characters, or "
                            "have been written by an incompatible version."));
    return;
  }

  m_filters = *filters;
  refreshList();
}

void TorrentFiltersPage::exportFilters() {
  applyCheckStates();

  if (m_filters.empty()) {
    QMessageBox::information(this, tr("Export Filters"), tr("There are no filters to export."));
    return;
  }

  textDialog(this, tr("Export Filters"), tr("Copy the text below and share it with other people:"),
             track::util::encodeFilters(m_filters), true);
}

void TorrentFiltersPage::resetFilters() {
  if (!confirm(this, tr("Are you sure you want to reset the filters?"),
               tr("All custom filters will be lost."), tr("Reset"))) {
    return;
  }

  m_filters.clear();
  for (const auto& preset : track::filterManager.presets()) {
    if (preset.is_default) m_filters.push_back(preset.filter);
  }
  refreshList();
}

void TorrentFiltersPage::refreshList() {
  const auto row = m_listFilters->currentRow();

  m_listFilters->clear();

  for (const auto& filter : m_filters) {
    const auto item = new QListWidgetItem(QString::fromStdString(filter.name), m_listFilters);
    item->setCheckState(filter.enabled ? Qt::Checked : Qt::Unchecked);
    item->setToolTip(describeFilter(filter));
  }

  m_listFilters->setCurrentRow(std::min(row < 0 ? 0 : row, m_listFilters->count() - 1));
  refreshState();
}

void TorrentFiltersPage::refreshState() {
  const auto row = m_listFilters->currentRow();

  m_buttonEdit->setEnabled(row > -1);
  m_buttonRemove->setEnabled(row > -1);
  m_buttonUp->setEnabled(row > 0);
  m_buttonDown->setEnabled(row > -1 && row < m_listFilters->count() - 1);
}

}  // namespace gui
