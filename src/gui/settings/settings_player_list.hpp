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

#include <QListWidget>

namespace gui {

// Lists media players of a given type, where unchecked items are disabled in settings
class PlayerListWidget final : public QListWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(PlayerListWidget)

public:
  PlayerListWidget(bool webBrowsers, QWidget* parent);
  ~PlayerListWidget() override = default;

  void load();
  void save() const;
};

}  // namespace gui
