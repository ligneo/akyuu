/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
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

#include <QCoreApplication>
#include <QThread>

namespace taiga {

class Orange final : public QThread {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(Orange)

public:
  Orange(QObject* parent);
  ~Orange();

protected:
  void run() override;
};

// The melody outlives the About dialog it is started from. v1 keeps it in a global
// (`taiga::orange`) for the same reason: closing the window should not cut the song off.
inline Orange* orange() {
  static auto orange = new Orange(qApp);
  return orange;
}

}  // namespace taiga
