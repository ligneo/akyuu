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
class QRadioButton;
class QSpinBox;

namespace gui {

class RecognitionPage final : public SettingsPage {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(RecognitionPage)

public:
  RecognitionPage(QWidget* parent);
  ~RecognitionPage() override = default;

  void load() override;
  void save() override;

private:
  QCheckBox* m_checkDetectionEnabled = nullptr;
  QSpinBox* m_spinDetectionInterval = nullptr;
  QCheckBox* m_checkOutOfRoot = nullptr;
  QCheckBox* m_checkOutOfRange = nullptr;
  QSpinBox* m_spinDelay = nullptr;
  QCheckBox* m_checkPauseWhenUnfocused = nullptr;
  QRadioButton* m_radioAfterDelay = nullptr;
  QRadioButton* m_radioOnPlayerClose = nullptr;
  QCheckBox* m_checkAskToConfirm = nullptr;
  QCheckBox* m_checkNotifyRecognized = nullptr;
  QCheckBox* m_checkNotifyNotRecognized = nullptr;
  QCheckBox* m_checkGoToRecognized = nullptr;
  QCheckBox* m_checkGoToNotRecognized = nullptr;
};

}  // namespace gui
