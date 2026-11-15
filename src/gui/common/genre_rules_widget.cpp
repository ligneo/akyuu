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

#include "genre_rules_widget.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "gui/utils/content_filter.hpp"

namespace gui {

using namespace Qt::StringLiterals;

namespace {

constexpr int kColumns = 4;

std::vector<std::string> toStrings(const QStringList& list) {
  std::vector<std::string> strings;
  for (const auto& s : list) strings.push_back(s.toStdString());
  return strings;
}

}  // namespace

GenreRulesWidget::GenreRulesWidget(QWidget* parent)
    : QWidget(parent),
      m_grid(new QGridLayout()),
      m_labelEmpty(new QLabel(tr("Genres appear here once a season or a search is loaded."), this)) {
  const auto layout = new QVBoxLayout(this);

  const auto hint = new QLabel(tr("Click a genre once to require it, twice to exclude it."), this);
  hint->setEnabled(false);
  layout->addWidget(hint);

  m_grid->setSpacing(4);
  layout->addLayout(m_grid);

  m_labelEmpty->setEnabled(false);
  layout->addWidget(m_labelEmpty);

  const auto buttons = new QHBoxLayout();
  buttons->addStretch();
  const auto reset = new QPushButton(tr("Reset"), this);
  buttons->addWidget(reset);
  layout->addLayout(buttons);

  connect(reset, &QPushButton::clicked, this, [this]() {
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
      it.value() = State::None;
      updateButton(m_buttons.value(it.key()), State::None);
    }
    emit rulesChanged();
  });

  setRules(contentFilter().genreRules());
}

anime::GenreRules GenreRulesWidget::rules() const {
  QStringList required;
  QStringList excluded;
  for (const auto& [genre, state] : m_states.asKeyValueRange()) {
    if (state == State::Required) required.push_back(genre);
    if (state == State::Excluded) excluded.push_back(genre);
  }
  return {.required = toStrings(required), .excluded = toStrings(excluded)};
}

void GenreRulesWidget::setRules(const anime::GenreRules& rules) {
  m_states.clear();
  for (const auto& genre : rules.required) m_states[QString::fromStdString(genre)] = State::Required;
  for (const auto& genre : rules.excluded) m_states[QString::fromStdString(genre)] = State::Excluded;
  reload();
}

void GenreRulesWidget::reload() {
  for (const auto button : std::as_const(m_buttons)) {
    m_grid->removeWidget(button);
    button->deleteLater();
  }
  m_buttons.clear();

  const auto genres = contentFilter().knownGenres();

  int index = 0;
  for (const auto& genre : genres) {
    if (!m_states.contains(genre)) m_states[genre] = State::None;

    const auto button = new QToolButton(this);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setAutoRaise(false);
    button->setProperty("genre", genre);
    updateButton(button, m_states.value(genre));
    m_grid->addWidget(button, index / kColumns, index % kColumns);
    m_buttons[genre] = button;
    ++index;

    connect(button, &QToolButton::clicked, this, [this, genre, button]() {
      auto& state = m_states[genre];
      state = state == State::None       ? State::Required
              : state == State::Required ? State::Excluded
                                         : State::None;
      updateButton(button, state);
      emit rulesChanged();
    });
  }

  m_labelEmpty->setVisible(genres.isEmpty());
}

void GenreRulesWidget::updateButton(QToolButton* button, const State state) const {
  const auto genre = button->property("genre").toString();
  auto font = button->font();
  font.setBold(state == State::Required);
  font.setStrikeOut(state == State::Excluded);
  button->setFont(font);

  switch (state) {
    case State::None:
      button->setText(genre);
      button->setToolTip(tr("Any"));
      break;
    case State::Required:
      button->setText(u"✓ %1"_s.arg(genre));
      button->setToolTip(tr("Required"));
      break;
    case State::Excluded:
      button->setText(u"✕ %1"_s.arg(genre));
      button->setToolTip(tr("Excluded"));
      break;
  }
}

QMenu* createGenresMenu(QWidget* parent) {
  const auto menu = new QMenu(parent);
  const auto widget = new GenreRulesWidget(menu);
  const auto action = new QWidgetAction(menu);
  action->setDefaultWidget(widget);
  menu->addAction(action);

  // Another page or the settings may have changed the rules since the menu was last open
  QObject::connect(menu, &QMenu::aboutToShow, widget,
                   [widget]() { widget->setRules(contentFilter().genreRules()); });
  QObject::connect(widget, &GenreRulesWidget::rulesChanged, widget,
                   [widget]() { contentFilter().setGenreRules(widget->rules()); });

  return menu;
}

}  // namespace gui
