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

#include <QString>

#include "gui/settings/settings_page.hpp"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;

namespace gui {

class IrcPage final : public SettingsPage {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(IrcPage)

public:
  IrcPage(QWidget* parent);
  ~IrcPage() override = default;

  void load() override;
  void save() override;

private:
  void updateStatus();

  QCheckBox* m_checkEnabled = nullptr;
  QRadioButton* m_radioAllChannels = nullptr;
  QRadioButton* m_radioCustomChannels = nullptr;
  QLineEdit* m_editChannels = nullptr;
  QCheckBox* m_checkUseAction = nullptr;
  QPushButton* m_buttonFormat = nullptr;
  QString m_format;
  QLabel* m_labelStatus = nullptr;
};

}  // namespace gui
