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

#include "settings_recognition_page.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "akyuu/settings.hpp"
#include "track/media.hpp"

namespace gui {

RecognitionPage::RecognitionPage(QWidget* parent)
    : SettingsPage(parent),
      m_checkDetectionEnabled(new QCheckBox(tr("Enable media detection"), this)),
      m_spinDetectionInterval(new QSpinBox(this)),
      m_checkOutOfRoot(new QCheckBox(tr("Ignore files outside of library folders"), this)),
      m_checkOutOfRange(
          new QCheckBox(tr("Ignore if episode number is greater than next episode"), this)),
      m_spinDelay(new QSpinBox(this)),
      m_checkPauseWhenUnfocused(
          new QCheckBox(tr("Pause delay while player is not in focus"), this)),
      m_radioAfterDelay(new QRadioButton(tr("Update after delay"), this)),
      m_radioOnPlayerClose(new QRadioButton(tr("Update when media is closed (after delay)"), this)),
      m_checkAskToConfirm(new QCheckBox(tr("Ask for confirmation"), this)),
      m_checkNotifyRecognized(new QCheckBox(tr("Notify me"), this)),
      m_checkNotifyNotRecognized(new QCheckBox(tr("Notify me"), this)),
      m_checkGoToRecognized(new QCheckBox(tr("Go to Now Playing"), this)),
      m_checkGoToNotRecognized(new QCheckBox(tr("Go to Now Playing"), this)) {
  const auto layout = new QVBoxLayout(this);

  layout->addWidget(m_checkDetectionEnabled);
  {
    const auto form = new QFormLayout();
    m_spinDetectionInterval->setRange(1, 60);
    m_spinDetectionInterval->setSuffix(tr(" seconds"));
    form->addRow(tr("Detection interval:"), m_spinDetectionInterval);
    layout->addLayout(form);
  }

  // Validation
  {
    const auto group = new QGroupBox(tr("Validation"), this);
    const auto groupLayout = new QVBoxLayout(group);
    groupLayout->addWidget(m_checkOutOfRoot);
    groupLayout->addWidget(m_checkOutOfRange);
    layout->addWidget(group);
  }

  // Behavior, laid out as in v1
  {
    const auto group = new QGroupBox(tr("Behavior"), this);
    const auto grid = new QGridLayout(group);
    grid->addWidget(new QLabel(tr("When an episode is recognized:"), group), 0, 0);
    grid->addWidget(m_checkNotifyRecognized, 1, 0);
    grid->addWidget(m_checkGoToRecognized, 2, 0);
    grid->addWidget(new QLabel(tr("When an episode is not recognized:"), group), 0, 1);
    grid->addWidget(m_checkNotifyNotRecognized, 1, 1);
    grid->addWidget(m_checkGoToNotRecognized, 2, 1);
    layout->addWidget(group);
  }

  // Anime list update
  {
    const auto group = new QGroupBox(tr("Anime list update"), this);
    const auto groupLayout = new QVBoxLayout(group);
    const auto form = new QFormLayout();
    m_spinDelay->setRange(akyuu::Settings::kUpdateDelayMin.count(),
                          akyuu::Settings::kUpdateDelayMax.count());
    m_spinDelay->setSingleStep(10);
    m_spinDelay->setSuffix(tr(" seconds"));
    form->addRow(tr("Delay:"), m_spinDelay);
    groupLayout->addLayout(form);
    groupLayout->addWidget(m_checkPauseWhenUnfocused);
    groupLayout->addWidget(m_radioAfterDelay);
    groupLayout->addWidget(m_radioOnPlayerClose);
    groupLayout->addWidget(m_checkAskToConfirm);
    layout->addWidget(group);
  }

  layout->addStretch();
}

void RecognitionPage::load() {
  m_checkDetectionEnabled->setChecked(akyuu::settings.mediaDetectionEnabled());
  const auto interval =
      std::chrono::duration_cast<std::chrono::seconds>(akyuu::settings.mediaDetectionInterval());
  m_spinDetectionInterval->setValue(static_cast<int>(interval.count()));
  m_checkOutOfRoot->setChecked(akyuu::settings.updateLibraryOnly());
  m_checkOutOfRange->setChecked(akyuu::settings.updateOutOfRange());
  m_spinDelay->setValue(static_cast<int>(akyuu::settings.updateDelay().count()));
  m_checkPauseWhenUnfocused->setChecked(akyuu::settings.updatePauseWhenUnfocused());
  (akyuu::settings.updateTrigger() == track::UpdateTrigger::OnPlayerClose ? m_radioOnPlayerClose
                                                                          : m_radioAfterDelay)
      ->setChecked(true);
  m_checkAskToConfirm->setChecked(akyuu::settings.updateAskToConfirm());
  m_checkNotifyRecognized->setChecked(akyuu::settings.syncNotifyRecognized());
  m_checkNotifyNotRecognized->setChecked(akyuu::settings.syncNotifyNotRecognized());
  m_checkGoToRecognized->setChecked(akyuu::settings.syncGoToNowPlayingRecognized());
  m_checkGoToNotRecognized->setChecked(akyuu::settings.syncGoToNowPlayingNotRecognized());
}

void RecognitionPage::save() {
  akyuu::settings.setMediaDetectionInterval(std::chrono::seconds{m_spinDetectionInterval->value()});
  akyuu::settings.setUpdateLibraryOnly(m_checkOutOfRoot->isChecked());
  akyuu::settings.setUpdateOutOfRange(m_checkOutOfRange->isChecked());
  akyuu::settings.setUpdateDelay(std::chrono::seconds{m_spinDelay->value()});
  akyuu::settings.setUpdatePauseWhenUnfocused(m_checkPauseWhenUnfocused->isChecked());
  akyuu::settings.setUpdateTrigger(m_radioOnPlayerClose->isChecked()
                                       ? track::UpdateTrigger::OnPlayerClose
                                       : track::UpdateTrigger::AfterDelay);
  akyuu::settings.setUpdateAskToConfirm(m_checkAskToConfirm->isChecked());
  akyuu::settings.setSyncNotifyRecognized(m_checkNotifyRecognized->isChecked());
  akyuu::settings.setSyncNotifyNotRecognized(m_checkNotifyNotRecognized->isChecked());
  akyuu::settings.setSyncGoToNowPlayingRecognized(m_checkGoToRecognized->isChecked());
  akyuu::settings.setSyncGoToNowPlayingNotRecognized(m_checkGoToNotRecognized->isChecked());

  // Must come after the interval is saved, since it restarts polling with the new interval.
  const auto enabled = m_checkDetectionEnabled->isChecked();
  akyuu::settings.setMediaDetectionEnabled(enabled);
  track::media::detection()->setEnabled(enabled);
}

}  // namespace gui
