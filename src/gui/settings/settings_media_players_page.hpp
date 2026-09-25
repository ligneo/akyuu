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

#pragma once

#include "gui/settings/settings_page.hpp"

namespace gui {

class PlayerListWidget;

class MediaPlayersPage final : public SettingsPage {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(MediaPlayersPage)

public:
  MediaPlayersPage(QWidget* parent);
  ~MediaPlayersPage() override = default;

  void load() override;
  void save() override;

private:
  PlayerListWidget* m_listPlayers = nullptr;
};

}  // namespace gui
