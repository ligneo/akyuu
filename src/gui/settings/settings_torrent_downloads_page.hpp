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
class QComboBox;
class QLineEdit;
class QPushButton;
class QRadioButton;

namespace gui {

class TorrentDownloadsPage final : public SettingsPage {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(TorrentDownloadsPage)

public:
  TorrentDownloadsPage(QWidget* parent);
  ~TorrentDownloadsPage() override = default;

  void load() override;
  void save() override;

private:
  void refreshState();

  QComboBox* m_comboSortBy = nullptr;
  QComboBox* m_comboSortOrder = nullptr;
  QCheckBox* m_checkUseAnimeFolder = nullptr;
  QCheckBox* m_checkFallback = nullptr;
  QLineEdit* m_editLocation = nullptr;
  QPushButton* m_buttonBrowseLocation = nullptr;
  QCheckBox* m_checkCreateSubfolder = nullptr;
  QCheckBox* m_checkOpen = nullptr;
  QRadioButton* m_radioDefaultApp = nullptr;
  QRadioButton* m_radioCustomApp = nullptr;
  QLineEdit* m_editAppPath = nullptr;
};

}  // namespace gui
