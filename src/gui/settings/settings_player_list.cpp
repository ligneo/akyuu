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

#include "settings_player_list.hpp"

#include <algorithm>
#include <set>

#include "akyuu/settings.hpp"
#include "track/media_player.hpp"

namespace gui {

PlayerListWidget::PlayerListWidget(bool webBrowsers, QWidget* parent) : QListWidget(parent) {
  std::vector<track::media::Player> players;
  track::media::parsePlayersData(players);

  const auto type =
      webBrowsers ? anisthesia::PlayerType::WebBrowser : anisthesia::PlayerType::Default;

  for (const auto& player : players) {
    if (player.type != type) continue;
    const auto item = new QListWidgetItem(QString::fromStdString(player.name), this);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
  }

  sortItems();
}

void PlayerListWidget::load() {
  const auto disabledPlayers = taiga::settings.disabledMediaPlayers();

  for (int i = 0; i < count(); ++i) {
    const auto name = item(i)->text().toStdString();
    const bool disabled = std::ranges::contains(disabledPlayers, name);
    item(i)->setCheckState(disabled ? Qt::Unchecked : Qt::Checked);
  }
}

void PlayerListWidget::save() const {
  // Other pages may list other players, so only the ones listed here are replaced
  std::set<std::string> listed;
  std::vector<std::string> disabledPlayers;

  for (int i = 0; i < count(); ++i) {
    const auto name = item(i)->text().toStdString();
    listed.insert(name);
    if (item(i)->checkState() == Qt::Unchecked) disabledPlayers.push_back(name);
  }

  for (const auto& name : taiga::settings.disabledMediaPlayers()) {
    if (!listed.contains(name)) disabledPlayers.push_back(name);
  }

  std::ranges::sort(disabledPlayers);
  taiga::settings.setDisabledMediaPlayers(disabledPlayers);
}

}  // namespace gui
