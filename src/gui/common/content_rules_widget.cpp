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

#include "content_rules_widget.hpp"

#include <algorithm>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QPalette>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "gui/utils/content_filter.hpp"
#include "gui/utils/format.hpp"

namespace gui {

using namespace Qt::StringLiterals;

namespace {

// Chips take the width of their text and wrap like words. A grid stretched every genre to its
// column, so short names sat in wide boxes.
class FlowLayout final : public QLayout {
public:
  explicit FlowLayout(QWidget* parent, int spacing) : QLayout(parent), m_spacing(spacing) {
    setContentsMargins(0, 0, 0, 0);
  }
  ~FlowLayout() override {
    while (const auto item = takeAt(0)) delete item;
  }

  void addItem(QLayoutItem* item) override { m_items.push_back(item); }
  int count() const override { return m_items.size(); }
  QLayoutItem* itemAt(int index) const override { return m_items.value(index); }
  QLayoutItem* takeAt(int index) override {
    return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
  }

  Qt::Orientations expandingDirections() const override { return {}; }
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override { return arrange({0, 0, width, 0}, true); }
  // Wrapped at the width the layout has, or at a usual one before it has any
  QSize sizeHint() const override {
    const int width = geometry().width() > 0 ? geometry().width() : 400;
    return {minimumSize().width(), heightForWidth(width)};
  }
  QSize minimumSize() const override {
    QSize size;
    for (const auto item : m_items) size = size.expandedTo(item->minimumSize());
    return size;
  }

  void setGeometry(const QRect& rect) override {
    QLayout::setGeometry(rect);
    arrange(rect, false);
  }

private:
  int arrange(const QRect& rect, bool measureOnly) const {
    int x = rect.x();
    int y = rect.y();
    int lineHeight = 0;
    for (const auto item : m_items) {
      const auto size = item->sizeHint();
      if (x + size.width() > rect.right() + 1 && lineHeight > 0) {
        x = rect.x();
        y += lineHeight + m_spacing;
        lineHeight = 0;
      }
      if (!measureOnly) item->setGeometry({QPoint{x, y}, size});
      x += size.width() + m_spacing;
      lineHeight = std::max(lineHeight, size.height());
    }
    return y + lineHeight - rect.y();
  }

  QList<QLayoutItem*> m_items;
  int m_spacing = 0;
};

// Drawn from the palette, so the chips follow the light and dark themes. The border is mixed from
// the text and the background: `palette(mid)` matched the background in some color schemes and
// left the chips without an outline. Excluded genres keep a warning color of their own, as the
// palette has none.
QString chipStyle(const QPalette& palette) {
  const auto text = palette.color(QPalette::WindowText);
  const auto base = palette.color(QPalette::Window);
  const auto mix = [&](double t) {
    return QColor::fromRgbF(base.redF() + (text.redF() - base.redF()) * t,
                            base.greenF() + (text.greenF() - base.greenF()) * t,
                            base.blueF() + (text.blueF() - base.blueF()) * t)
        .name();
  };
  return uR"(
QScrollArea#genreScroll, QScrollArea#genreScroll > QWidget > QWidget {
  background: transparent;
}
QToolButton#genreChip {
  border: 1px solid %1;
  border-radius: 11px;
  padding: 3px 10px;
  background: transparent;
}
QToolButton#genreChip:hover {
  border-color: palette(highlight);
}
QToolButton#genreChip[state="required"] {
  background: palette(highlight);
  border-color: palette(highlight);
  color: palette(highlighted-text);
}
QToolButton#genreChip[state="excluded"] {
  border-color: #c4505c;
  color: #e0737f;
}
)"_s.arg(mix(0.35));
}

std::vector<std::string> toStrings(const QStringList& list) {
  std::vector<std::string> strings;
  for (const auto& s : list) strings.push_back(s.toStdString());
  return strings;
}

}  // namespace

ContentRulesWidget::ContentRulesWidget(QWidget* parent)
    : QWidget(parent),
      m_labelEmpty(new QLabel(tr("Genres appear here once a season or a search is loaded."), this)) {
  setStyleSheet(chipStyle(palette()));

  const auto layout = new QVBoxLayout(this);
  layout->setSpacing(10);

  // Types first: there are only a few, they never change and they need no scrolling
  {
    const auto types = new QWidget(this);
    const auto flow = new FlowLayout(types, 6);
    for (const auto type : anime::kTypes) {
      m_typeStates[type] = State::None;
      const auto chip = new QToolButton(types);
      chip->setObjectName("genreChip");
      chip->setCursor(Qt::PointingHandCursor);
      chip->setProperty("label", formatType(type));
      updateChip(chip, State::None);
      flow->addWidget(chip);
      m_typeButtons[type] = chip;

      connect(chip, &QToolButton::clicked, this, [this, type, chip]() {
        auto& state = m_typeStates[type];
        state = state == State::None ? State::Excluded : State::None;
        updateChip(chip, state);
        emit rulesChanged();
      });
    }
    layout->addWidget(types);

    const auto line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setEnabled(false);
    layout->addWidget(line);
  }

  m_labelHint = new QLabel(tr("Click a genre once to require it, twice to exclude it."), this);
  m_labelHint->setWordWrap(true);
  m_labelHint->setEnabled(false);
  layout->addWidget(m_labelHint);

  // MyAnimeList sends its themes and demographics as genres too, which makes for dozens of
  // chips. Past a few rows they scroll rather than stretch the menu down the screen.
  m_scroll = new QScrollArea(this);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setWidgetResizable(true);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setObjectName("genreScroll");
  m_scroll->viewport()->setAutoFillBackground(false);
  const auto chips = new QWidget(m_scroll);
  chips->setAutoFillBackground(false);
  m_chips = new FlowLayout(chips, 6);
  m_scroll->setWidget(chips);
  layout->addWidget(m_scroll);

  m_labelEmpty->setEnabled(false);
  layout->addWidget(m_labelEmpty);

  const auto buttons = new QHBoxLayout();
  m_buttonDefaults = new QPushButton(tr("Back to default"), this);
  m_buttonDefaults->hide();
  buttons->addWidget(m_buttonDefaults);
  buttons->addStretch();
  const auto reset = new QPushButton(tr("Reset"), this);
  buttons->addWidget(reset);
  layout->addLayout(buttons);

  connect(m_buttonDefaults, &QPushButton::clicked, this, &ContentRulesWidget::defaultsRequested);

  connect(reset, &QPushButton::clicked, this, [this]() {
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
      it.value() = State::None;
      if (const auto chip = m_buttons.value(it.key())) updateChip(chip, State::None);
    }
    for (auto it = m_typeStates.begin(); it != m_typeStates.end(); ++it) {
      it.value() = State::None;
      updateChip(m_typeButtons.value(it.key()), State::None);
    }
    emit rulesChanged();
  });

  setFilters(contentFilter().genreRules(), contentFilter().hiddenTypes());
}

anime::GenreRules ContentRulesWidget::rules() const {
  QStringList required;
  QStringList excluded;
  for (const auto& [genre, state] : m_states.asKeyValueRange()) {
    if (state == State::Required) required.push_back(genre);
    if (state == State::Excluded) excluded.push_back(genre);
  }
  return {.required = toStrings(required), .excluded = toStrings(excluded)};
}

std::vector<anime::Type> ContentRulesWidget::hiddenTypes() const {
  std::vector<anime::Type> types;
  for (const auto type : anime::kTypes) {
    if (m_typeStates.value(type) == State::Excluded) types.push_back(type);
  }
  return types;
}

void ContentRulesWidget::setFilters(const anime::GenreRules& rules,
                                    const std::vector<anime::Type>& types) {
  for (const auto type : anime::kTypes) {
    const auto state = std::ranges::contains(types, type) ? State::Excluded : State::None;
    m_typeStates[type] = state;
    updateChip(m_typeButtons.value(type), state);
  }

  m_states.clear();
  for (const auto& genre : rules.required) m_states[QString::fromStdString(genre)] = State::Required;
  for (const auto& genre : rules.excluded) m_states[QString::fromStdString(genre)] = State::Excluded;
  reload();
}

void ContentRulesWidget::reload() {
  for (const auto chip : std::as_const(m_buttons)) {
    m_chips->removeWidget(chip);
    chip->deleteLater();
  }
  m_buttons.clear();

  const auto genres = contentFilter().knownGenres();
  const auto parent = m_chips->parentWidget();

  for (const auto& genre : genres) {
    if (!m_states.contains(genre)) m_states[genre] = State::None;

    const auto chip = new QToolButton(parent);
    chip->setObjectName("genreChip");
    chip->setCursor(Qt::PointingHandCursor);
    chip->setProperty("label", genre);
    updateChip(chip, m_states.value(genre));
    m_chips->addWidget(chip);
    m_buttons[genre] = chip;

    connect(chip, &QToolButton::clicked, this, [this, genre, chip]() {
      auto& state = m_states[genre];
      state = state == State::None       ? State::Required
              : state == State::Required ? State::Excluded
                                         : State::None;
      updateChip(chip, state);
      emit rulesChanged();
    });
  }

  m_labelEmpty->setVisible(genres.isEmpty());
  fitHeight();
}

// The menu keeps to the chips: its button already says when the rules differ from the
// defaults, and the way back only shows up then.
void ContentRulesWidget::setTemporary(const bool temporary) {
  m_labelHint->setVisible(!temporary);
  m_buttonDefaults->setVisible(temporary && contentFilter().filtersChanged());
}

void ContentRulesWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  fitHeight();
}

void ContentRulesWidget::fitHeight() {
  constexpr int kMaxRows = 6;
  // From this widget's own width: the scroll area is laid out after this runs, so its viewport
  // still has the old width and the chips seemed to need more rows than they do.
  const auto margins = layout()->contentsMargins();
  const int available = width() - margins.left() - margins.right();
  int needed = m_chips->heightForWidth(available);
  const int row = m_buttons.isEmpty() ? 0 : m_buttons.first()->sizeHint().height() + 6;
  const int maxHeight = row > 0 ? (row * kMaxRows) - 6 : needed;
  // Scrolling takes the width of the scroll bar from the chips
  if (needed > maxHeight) {
    needed = m_chips->heightForWidth(available - m_scroll->verticalScrollBar()->sizeHint().width());
  }
  const int height = std::min(needed, maxHeight);
  if (m_scroll->height() != height) {
    m_scroll->setFixedHeight(height);
    m_scroll->setVerticalScrollBarPolicy(needed > height ? Qt::ScrollBarAsNeeded
                                                         : Qt::ScrollBarAlwaysOff);
    updateGeometry();
  }
}

void ContentRulesWidget::updateChip(QToolButton* chip, const State state) const {
  // A button takes "&" for a shortcut marker, so a name with one is doubled to keep it
  const auto genre = chip->property("label").toString().replace(u'&', u"&&"_s);
  auto font = chip->font();
  font.setStrikeOut(state == State::Excluded);
  chip->setFont(font);

  switch (state) {
    case State::None:
      chip->setText(genre);
      chip->setProperty("state", u"none"_s);
      break;
    case State::Required:
      chip->setText(u"✓ %1"_s.arg(genre));
      chip->setProperty("state", u"required"_s);
      break;
    case State::Excluded:
      chip->setText(u"✕ %1"_s.arg(genre));
      chip->setProperty("state", u"excluded"_s);
      break;
  }

  // A dynamic property only restyles the widget once the style looks at it again
  chip->style()->unpolish(chip);
  chip->style()->polish(chip);
}

QMenu* createFiltersMenu(QWidget* parent) {
  const auto menu = new QMenu(parent);
  const auto widget = new ContentRulesWidget(menu);
  // Without a width, the chips would line up in one row as wide as the screen. It follows the
  // font, as a fixed one cut the hint short at larger sizes.
  widget->setFixedWidth(widget->fontMetrics().averageCharWidth() * 56);
  const auto action = new QWidgetAction(menu);
  action->setDefaultWidget(widget);
  menu->addAction(action);

  // Another page or the settings may have changed the rules since the menu was last open
  QObject::connect(menu, &QMenu::aboutToShow, widget, [widget]() {
    widget->setFilters(contentFilter().genreRules(), contentFilter().hiddenTypes());
    widget->setTemporary(true);
    // The menu asks for one height and does not follow the wrapping, so it is given the height
    // the chips need at this width. The size hint guessed the width of the type chips before they
    // had one and left a gap under them.
    widget->fitHeight();
    widget->setFixedHeight(widget->heightForWidth(widget->width()));
  });
  QObject::connect(widget, &ContentRulesWidget::rulesChanged, widget, [widget]() {
    contentFilter().setFilters(widget->rules(), widget->hiddenTypes());
    widget->setTemporary(true);
  });
  QObject::connect(widget, &ContentRulesWidget::defaultsRequested, widget, [widget]() {
    contentFilter().setFilters(contentFilter().defaultGenreRules(),
                               contentFilter().defaultHiddenTypes());
    widget->setFilters(contentFilter().genreRules(), contentFilter().hiddenTypes());
    widget->setTemporary(true);
  });

  return menu;
}

QString filtersButtonText() {
  const auto& rules = contentFilter().genreRules();
  const auto count =
      contentFilter().hiddenTypes().size() + rules.required.size() + rules.excluded.size();
  auto text = count > 0 ? QObject::tr("Filters (%1)").arg(count) : QObject::tr("Filters");
  // A dot says the rules are changed for now, away from the ones in the settings
  if (contentFilter().filtersChanged()) text += u" •"_s;
  return text;
}

}  // namespace gui
