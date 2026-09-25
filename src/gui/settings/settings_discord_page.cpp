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

#include "settings_discord_page.hpp"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include "akyuu/settings.hpp"
#include "link/discord.hpp"

namespace gui {

DiscordPage::DiscordPage(QWidget* parent)
    : SettingsPage(parent),
      m_checkEnabled(new QCheckBox(tr("Update rich presence"), this)),
      m_checkTime(new QCheckBox(tr("Display elapsed time"), this)),
      m_checkUsername(new QCheckBox(tr("Display username in tooltip"), this)),
      m_checkGroup(new QCheckBox(tr("Display episode release group"), this)) {
  const auto layout = new QVBoxLayout(this);

  const auto group = new QGroupBox(tr("Options"), this);
  const auto groupLayout = new QVBoxLayout(group);

  layout->addWidget(m_checkEnabled);
  groupLayout->addWidget(m_checkTime);
  groupLayout->addWidget(m_checkUsername);
  groupLayout->addWidget(m_checkGroup);

  const auto note = new QLabel(tr("Note: Requires using the Discord desktop client."), group);
  note->setWordWrap(true);
  groupLayout->addWidget(note);

  layout->addWidget(group);
  layout->addStretch();

  connect(m_checkEnabled, &QCheckBox::toggled, m_checkTime, &QWidget::setEnabled);
  connect(m_checkEnabled, &QCheckBox::toggled, m_checkUsername, &QWidget::setEnabled);
  connect(m_checkEnabled, &QCheckBox::toggled, m_checkGroup, &QWidget::setEnabled);
}

void DiscordPage::load() {
  m_checkEnabled->setChecked(akyuu::settings.discordEnabled());
  m_checkTime->setChecked(akyuu::settings.discordTimeEnabled());
  m_checkUsername->setChecked(akyuu::settings.discordUsernameEnabled());
  m_checkGroup->setChecked(akyuu::settings.discordGroupEnabled());
  m_checkTime->setEnabled(m_checkEnabled->isChecked());
  m_checkUsername->setEnabled(m_checkEnabled->isChecked());
  m_checkGroup->setEnabled(m_checkEnabled->isChecked());
}

void DiscordPage::save() {
  akyuu::settings.setDiscordEnabled(m_checkEnabled->isChecked());
  akyuu::settings.setDiscordTimeEnabled(m_checkTime->isChecked());
  akyuu::settings.setDiscordUsernameEnabled(m_checkUsername->isChecked());
  akyuu::settings.setDiscordGroupEnabled(m_checkGroup->isChecked());

  link::discord()->applySettings();
}

}  // namespace gui
