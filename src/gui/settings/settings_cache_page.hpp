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

#include "gui/settings/settings_page.hpp"

class QCheckBox;
class QLabel;
class QPushButton;

namespace gui {

class CachePage final : public SettingsPage {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(CachePage)

public:
  CachePage(QWidget* parent);
  ~CachePage() override = default;

  void load() override;
  void save() override;

protected:
  void showEvent(QShowEvent* event) override;

private:
  void clear();
  void refresh();

  QCheckBox* m_checkHistory = nullptr;
  QLabel* m_labelHistory = nullptr;
  QCheckBox* m_checkImages = nullptr;
  QLabel* m_labelImages = nullptr;
  QCheckBox* m_checkTorrentFiles = nullptr;
  QLabel* m_labelTorrentFiles = nullptr;
  QCheckBox* m_checkTorrentHistory = nullptr;
  QLabel* m_labelTorrentHistory = nullptr;
  QPushButton* m_buttonClear = nullptr;
};

}  // namespace gui
