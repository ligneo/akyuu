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

#include <QCoreApplication>
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <ctime>

namespace link {

// Discord's rich presence, spoken directly over its local socket. v1 links against discord-rpc;
// the protocol is a handshake and one command, so there is no need for the library here.
class Discord final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(Discord)

public:
  Discord(QObject* parent);

  void applySettings();
  void updatePresence(QString details, QString state, const QString& largeImage,
                      const std::time_t timestamp, const bool force = false);
  void clearPresence();

private:
  bool connectToDiscord();
  void send(const int opcode, const QJsonObject& payload);

  QLocalSocket socket_{this};
};

inline Discord* discord() {
  static auto instance = new Discord(qApp);
  return instance;
}

}  // namespace link
