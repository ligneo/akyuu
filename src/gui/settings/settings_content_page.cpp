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

#include "settings_content_page.hpp"

#include <QButtonGroup>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

#include "gui/common/genre_rules_widget.hpp"
#include "gui/utils/content_filter.hpp"

namespace gui {

ContentPage::ContentPage(QWidget* parent)
    : SettingsPage(parent),
      m_groupAdult(new QButtonGroup(this)),
      m_labelAdult(new QLabel(this)),
      m_genres(new GenreRulesWidget(this)) {
  const auto layout = new QVBoxLayout(this);

  // Adult titles
  {
    const auto group = new QGroupBox(tr("Adult titles"), this);
    const auto groupLayout = new QVBoxLayout(group);

    // Three buttons side by side rather than a combo box, so that all three choices are in sight
    const auto segment = new QHBoxLayout();
    segment->setSpacing(0);
    const QList<QPair<QString, akyuu::AdultTitles>> modes{
        {tr("Show"), akyuu::AdultTitles::Show},
        {tr("Blur"), akyuu::AdultTitles::Blur},
        {tr("Hide"), akyuu::AdultTitles::Hide},
    };
    for (const auto& [text, mode] : modes) {
      const auto button = new QPushButton(text, group);
      button->setCheckable(true);
      m_groupAdult->addButton(button, static_cast<int>(mode));
      segment->addWidget(button);
    }
    segment->addStretch();
    groupLayout->addLayout(segment);

    m_labelAdult->setWordWrap(true);
    m_labelAdult->setEnabled(false);
    groupLayout->addWidget(m_labelAdult);

    connect(m_groupAdult, &QButtonGroup::idClicked, this, &ContentPage::updateDescription);

    layout->addWidget(group);
  }

  // Genres
  {
    const auto group = new QGroupBox(tr("Genres"), this);
    const auto groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(0, 0, 0, 0);
    groupLayout->addWidget(m_genres);
    layout->addWidget(group);
  }

  const auto note =
      new QLabel(tr("These apply to Seasons and Search. Your anime list always shows everything."),
                 this);
  note->setWordWrap(true);
  note->setEnabled(false);
  layout->addWidget(note);

  layout->addStretch();
}

void ContentPage::load() {
  m_groupAdult->button(static_cast<int>(contentFilter().adultTitles()))->setChecked(true);
  updateDescription();
  m_genres->setRules(contentFilter().genreRules());
}

void ContentPage::save() {
  contentFilter().setAdultTitles(static_cast<akyuu::AdultTitles>(m_groupAdult->checkedId()));
  contentFilter().setGenreRules(m_genres->rules());
}

void ContentPage::updateDescription() {
  // A checked push button looks barely pressed in Fusion, so the chosen one takes the accent color
  for (const auto button : m_groupAdult->buttons()) {
    auto palette = QPalette{};
    if (button->isChecked()) {
      palette.setColor(QPalette::Button, palette.color(QPalette::Highlight));
      palette.setColor(QPalette::ButtonText, palette.color(QPalette::HighlightedText));
    }
    button->setPalette(palette);
  }

  switch (static_cast<akyuu::AdultTitles>(m_groupAdult->checkedId())) {
    case akyuu::AdultTitles::Show:
      m_labelAdult->setText(tr("Adult titles are listed like any other."));
      break;
    case akyuu::AdultTitles::Blur:
      m_labelAdult->setText(
          tr("Adult titles are listed with their covers blurred. Click a cover to reveal it."));
      break;
    case akyuu::AdultTitles::Hide:
      m_labelAdult->setText(tr("Adult titles are left out."));
      break;
  }
}

}  // namespace gui
