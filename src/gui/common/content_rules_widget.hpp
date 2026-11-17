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

#include <QMap>
#include <QWidget>

#include "media/genre_rules.hpp"

class QLabel;
class QLayout;
class QMenu;
class QPushButton;
class QScrollArea;
class QToolButton;

namespace gui {

// One chip per type above one chip per genre. A click requires the type or genre, a second click
// excludes it and a third leaves it alone again.
class ContentRulesWidget final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ContentRulesWidget)

public:
  ContentRulesWidget(QWidget* parent);
  ~ContentRulesWidget() override = default;

  anime::GenreRules rules() const;
  anime::TypeRules typeRules() const;
  void setFilters(const anime::GenreRules& rules, const anime::TypeRules& types);

  // The genres come from the titles Akyuu knows, which grow with every season or search
  void reload();
  // The chips take the rows they need, up to a few, and scroll past them
  void fitHeight();

  // In a toolbar menu the rules last until Akyuu is closed. The hint stays with the settings,
  // and a way back to the defaults shows up once they differ.
  void setTemporary(bool temporary);

signals:
  void rulesChanged();
  void defaultsRequested();

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  enum class State {
    None,
    Required,
    Excluded,
  };

  static State nextState(State state);
  QToolButton* createChip(QWidget* parent, const QString& label) const;
  void updateChip(QToolButton* chip, State state) const;

  QMap<anime::Type, State> m_typeStates;
  QMap<anime::Type, QToolButton*> m_typeButtons;
  QLayout* m_chips = nullptr;
  QScrollArea* m_scroll = nullptr;
  QLabel* m_labelEmpty = nullptr;
  QLabel* m_labelHint = nullptr;
  QPushButton* m_buttonDefaults = nullptr;
  QMap<QString, State> m_states;
  QMap<QString, QToolButton*> m_buttons;
};

// The type and genre chips in a toolbar menu. A change applies at once, and the menu stays open
// for the next one.
QMenu* createFiltersMenu(QWidget* parent);

// "Filters" with the number of types and genres that are required or excluded, so that a rule
// left on is not forgotten
QString filtersButtonText();

}  // namespace gui
