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

#include "settings_streaming_page.hpp"

#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>

#include "akyuu/settings.hpp"
#include "gui/settings/settings_player_list.hpp"
#include "gui/settings/settings_stream_list.hpp"

namespace gui {

StreamingPage::StreamingPage(QWidget* parent)
    : SettingsPage(parent),
      m_checkEnabled(new QCheckBox(tr("Detect streaming media in web browsers"), this)),
      m_listPlayers(new PlayerListWidget(true, this)),
      m_listProviders(new StreamListWidget(this)) {
  const auto layout = new QVBoxLayout(this);

  layout->addWidget(m_checkEnabled);

  const auto browsersGroup = new QGroupBox(tr("Web browsers"), this);
  (new QVBoxLayout(browsersGroup))->addWidget(m_listPlayers);
  layout->addWidget(browsersGroup);

  const auto providersGroup = new QGroupBox(tr("Media providers"), this);
  (new QVBoxLayout(providersGroup))->addWidget(m_listProviders);
  layout->addWidget(providersGroup);

  // Enabled by load() through the checkbox
  browsersGroup->setEnabled(false);
  providersGroup->setEnabled(false);
  connect(m_checkEnabled, &QCheckBox::toggled, browsersGroup, &QWidget::setEnabled);
  connect(m_checkEnabled, &QCheckBox::toggled, providersGroup, &QWidget::setEnabled);
}

void StreamingPage::load() {
  m_checkEnabled->setChecked(akyuu::settings.streamingMediaEnabled());
  m_listPlayers->load();
  m_listProviders->load();
}

void StreamingPage::save() {
  akyuu::settings.setStreamingMediaEnabled(m_checkEnabled->isChecked());
  m_listPlayers->save();
  m_listProviders->save();
}

}  // namespace gui
