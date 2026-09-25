/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
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

#include "settings_anime_list_page.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "media/anime.hpp"
#include "akyuu/settings.hpp"

namespace gui {

using namespace Qt::StringLiterals;

AnimeListPage::AnimeListPage(QWidget* parent)
    : SettingsPage(parent),
      m_comboTitleLanguage(new QComboBox(this)),
      m_comboDoubleClick(new QComboBox(this)),
      m_comboMiddleClick(new QComboBox(this)),
      m_checkHighlight(new QCheckBox(
          tr("Highlight anime if next episode is available in library folders"), this)),
      m_checkHighlightedOnTop(new QCheckBox(tr("Display highlighted anime above others"), this)),
      m_checkShowAired(new QCheckBox(tr("Display aired episodes (estimated)"), this)),
      m_checkShowAvailable(
          new QCheckBox(tr("Display available episodes in library folders"), this)) {
  const auto layout = new QVBoxLayout(this);

  // Actions
  {
    const auto group = new QGroupBox(tr("Actions"), this);
    const auto form = new QFormLayout(group);

    // v1 offers six; its "edit details" and "view anime info" open the same dialog here.
    const QList<QPair<QString, QString>> actions{
        {tr("Do nothing"), u"none"_s},           {tr("View anime info"), u"details"_s},
        {tr("Open folder"), u"openFolder"_s},    {tr("Play next episode"), u"playNextEpisode"_s},
        {tr("View anime page"), u"animePage"_s},
    };

    for (const auto& [text, value] : actions) {
      m_comboDoubleClick->addItem(text, value);
      m_comboMiddleClick->addItem(text, value);
    }

    form->addRow(tr("Double click:"), m_comboDoubleClick);
    form->addRow(tr("Middle click:"), m_comboMiddleClick);

    layout->addWidget(group);
  }

  // Appearance
  {
    const auto group = new QGroupBox(tr("Appearance"), this);
    const auto form = new QFormLayout(group);

    m_comboTitleLanguage->addItem(tr("Romaji"), static_cast<int>(anime::TitleLanguage::Romaji));
    m_comboTitleLanguage->addItem(tr("English"), static_cast<int>(anime::TitleLanguage::English));
    m_comboTitleLanguage->addItem(tr("Native"), static_cast<int>(anime::TitleLanguage::Native));
    form->addRow(tr("Title language preference:"), m_comboTitleLanguage);

    form->addRow(m_checkHighlight);
    // Indented under the option it depends on, as in v1
    const auto indented = new QHBoxLayout();
    indented->addSpacing(20);
    indented->addWidget(m_checkHighlightedOnTop);
    form->addRow(indented);
    connect(m_checkHighlight, &QCheckBox::toggled, m_checkHighlightedOnTop, &QWidget::setEnabled);

    layout->addWidget(group);
  }

  // Progress
  {
    const auto group = new QGroupBox(tr("Progress"), this);
    const auto groupLayout = new QVBoxLayout(group);
    groupLayout->addWidget(m_checkShowAired);
    groupLayout->addWidget(m_checkShowAvailable);
    layout->addWidget(group);
  }

  layout->addStretch();
}

void AnimeListPage::load() {
  const auto language = static_cast<int>(taiga::settings.titleLanguage());
  m_comboTitleLanguage->setCurrentIndex(m_comboTitleLanguage->findData(language));

  m_comboDoubleClick->setCurrentIndex(std::max(
      m_comboDoubleClick->findData(QString::fromStdString(taiga::settings.listDoubleClickAction())),
      0));
  m_comboMiddleClick->setCurrentIndex(std::max(
      m_comboMiddleClick->findData(QString::fromStdString(taiga::settings.listMiddleClickAction())),
      0));

  m_checkHighlight->setChecked(taiga::settings.listHighlightNewEpisodes());
  m_checkHighlightedOnTop->setChecked(taiga::settings.listHighlightedOnTop());
  m_checkHighlightedOnTop->setEnabled(m_checkHighlight->isChecked());
  m_checkShowAired->setChecked(taiga::settings.listShowAiredEpisodes());
  m_checkShowAvailable->setChecked(taiga::settings.listShowAvailableEpisodes());
}

void AnimeListPage::save() {
  const auto language =
      static_cast<anime::TitleLanguage>(m_comboTitleLanguage->currentData().toInt());
  taiga::settings.setTitleLanguage(language);

  taiga::settings.setListDoubleClickAction(
      m_comboDoubleClick->currentData().toString().toStdString());
  taiga::settings.setListMiddleClickAction(
      m_comboMiddleClick->currentData().toString().toStdString());
  taiga::settings.setListHighlightNewEpisodes(m_checkHighlight->isChecked());
  taiga::settings.setListHighlightedOnTop(m_checkHighlightedOnTop->isChecked());
  taiga::settings.setListShowAiredEpisodes(m_checkShowAired->isChecked());
  taiga::settings.setListShowAvailableEpisodes(m_checkShowAvailable->isChecked());
}

}  // namespace gui
