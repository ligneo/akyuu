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

#include "media_player.hpp"

#include <algorithm>

#include "akyuu/path.hpp"
#include "akyuu/settings.hpp"
#include "base/file.hpp"
#include "base/string.hpp"

namespace track::media {

namespace {

bool isDisabled(const Player& player) {
  if (player.type == anisthesia::PlayerType::WebBrowser &&
      !akyuu::settings.streamingMediaEnabled()) {
    return true;
  }

  const auto disabledPlayers = akyuu::settings.disabledMediaPlayers();

  return std::ranges::any_of(disabledPlayers, [&player](const std::string& name) {
    return compareStrings(player.name, name, Qt::CaseInsensitive) == 0;
  });
}

// The bundled data can be extended or corrected by a file in the data directory, as in v1. A player
// with the same name replaces the bundled one, the rest are added.
void mergeUserPlayersData(std::vector<Player>& players) {
  const auto path = u"%1/players.anisthesia"_s.arg(QString::fromStdString(akyuu::get_data_path()));
  const auto file = base::readFile(path);

  if (file.isEmpty()) {
    return;
  }

  std::vector<Player> userPlayers;

  if (!anisthesia::ParsePlayersData(file.toStdString(), userPlayers)) {
    qCritical() << "Could not read" << path;
    return;
  }

  for (const auto& player : userPlayers) {
    const auto it = std::ranges::find(players, player.name, &Player::name);
    if (it != players.end()) {
      *it = player;
    } else {
      players.push_back(player);
    }
  }

  qDebug() << "Merged" << userPlayers.size() << "media players from" << path;
}

}  // namespace

bool parsePlayersData(std::vector<Player>& players) {
  const auto file = base::readFile(":/players.anisthesia");

  if (file.isEmpty()) {
    return false;
  }

  if (!anisthesia::ParsePlayersData(file.toStdString(), players)) {
    return false;
  }

  mergeUserPlayersData(players);

  return true;
}

std::vector<Player> getEnabledPlayers(const std::vector<Player>& players) {
  std::vector<Player> enabledPlayers;

  for (const auto player : players) {
    if (isDisabled(player)) continue;

    enabledPlayers.emplace_back(player);
  }

  return enabledPlayers;
}

}  // namespace track::media
