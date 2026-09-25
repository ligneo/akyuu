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

namespace gui {

// Edits a single filter condition. The operators and the values on offer depend on the element,
// so that a numeric element is not compared with "begins with", as in v1.
class FilterConditionDialog final : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(FilterConditionDialog)

public:
  FilterConditionDialog(QWidget* parent, const track::FilterCondition& condition);
  ~FilterConditionDialog() override = default;

  static std::optional<track::FilterCondition> edit(QWidget* parent,
                                                    const track::FilterCondition& condition = {});

  track::FilterCondition condition() const;

private:
  void chooseElement();

  QComboBox* m_comboElement = nullptr;
  QComboBox* m_comboOperator = nullptr;
  QComboBox* m_comboValue = nullptr;
};

}  // namespace gui
