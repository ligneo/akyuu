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

#include <vector>

#include "gui/settings/settings_page.hpp"
#include "track/feed_filter.hpp"

class QCheckBox;
class QTreeWidget;
class QPushButton;

namespace gui {

class TorrentFiltersPage final : public SettingsPage {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(TorrentFiltersPage)

public:
  TorrentFiltersPage(QWidget* parent);
  ~TorrentFiltersPage() override = default;

  void load() override;
  void save() override;

private:
  void applyCheckStates();
  void addFilter();
  void editFilter();
  void removeFilter();
  void moveFilter(const int offset);
  void importFilters();
  void exportFilters();
  void resetFilters();
  void refreshList();
  void refreshState();
  int currentRow() const;
  void setCurrentRow(const int row);

  std::vector<track::Filter> m_filters;

  QCheckBox* m_checkEnabled = nullptr;
  QTreeWidget* m_listFilters = nullptr;
  QPushButton* m_buttonEdit = nullptr;
  QPushButton* m_buttonRemove = nullptr;
  QPushButton* m_buttonUp = nullptr;
  QPushButton* m_buttonDown = nullptr;
};

}  // namespace gui
