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

#include <QDialog>
#include <optional>

#include "track/feed_filter.hpp"

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QTreeWidget;
class QPushButton;

namespace gui {

// Edits one filter: its name, what it does, and the conditions it is made of. v1 spreads this
// over the last two pages of a wizard; the preset choice of its first page is a dialog of its own.
class FilterDialog final : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(FilterDialog)

public:
  FilterDialog(QWidget* parent, const track::Filter& filter);
  ~FilterDialog() override = default;

  static std::optional<track::Filter> edit(QWidget* parent, const track::Filter& filter);

  track::Filter filter() const;

private:
  void addCondition();
  void editCondition();
  void removeCondition();
  void moveCondition(const int offset);
  void refreshConditions();
  void refreshState();
  int currentConditionRow() const;
  void setCurrentConditionRow(const int row);

  track::Filter m_filter;

  QLineEdit* m_editName = nullptr;
  QComboBox* m_comboAction = nullptr;
  QComboBox* m_comboMatch = nullptr;
  QComboBox* m_comboOption = nullptr;
  QLabel* m_labelOption = nullptr;
  QTreeWidget* m_listConditions = nullptr;
  QListWidget* m_listAnime = nullptr;
  QPushButton* m_buttonEdit = nullptr;
  QPushButton* m_buttonRemove = nullptr;
  QPushButton* m_buttonUp = nullptr;
  QPushButton* m_buttonDown = nullptr;
  QPushButton* m_buttonOk = nullptr;
};

// Lets the user start from one of the preset filters, as v1's first wizard page does.
std::optional<track::Filter> chooseFilterPreset(QWidget* parent);

}  // namespace gui
