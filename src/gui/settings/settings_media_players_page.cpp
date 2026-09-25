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

#include "settings_media_players_page.hpp"

#include <QLabel>
#include <QVBoxLayout>

#include "gui/settings/settings_player_list.hpp"

namespace gui {

MediaPlayersPage::MediaPlayersPage(QWidget* parent)
    : SettingsPage(parent), m_listPlayers(new PlayerListWidget(false, this)) {
  const auto layout = new QVBoxLayout(this);

  layout->addWidget(new QLabel(tr("Detect the following media players:"), this));
  layout->addWidget(m_listPlayers);
}

void MediaPlayersPage::load() {
  m_listPlayers->load();
}

void MediaPlayersPage::save() {
  m_listPlayers->save();
}

}  // namespace gui
