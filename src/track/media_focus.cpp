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

#include "media_focus.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

#include "base/string.hpp"

namespace track::media {

namespace {

// Wayland has no common way to tell which window is focused, so we have to ask the compositor.
// See: https://wiki.hypr.land/IPC/
std::optional<int> getFocusedProcessIdFromHyprland() {
  const auto signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
  if (signature.isEmpty()) return std::nullopt;

  const auto path =
      u"%1/hypr/%2/.socket.sock"_s.arg(qEnvironmentVariable("XDG_RUNTIME_DIR"), signature);

  QLocalSocket socket;
  socket.connectToServer(path);
  if (!socket.waitForConnected(100)) return std::nullopt;

  socket.write("j/activewindow");
  if (!socket.waitForBytesWritten(100)) return std::nullopt;

  // Hyprland closes the connection after sending the reply
  QByteArray data;
  while (socket.waitForReadyRead(100)) {
    data += socket.readAll();
  }
  data += socket.readAll();

  const auto pid = QJsonDocument::fromJson(data).object().value("pid");
  if (!pid.isDouble()) return std::nullopt;

  return pid.toInt();
}

}  // namespace

std::optional<int> getFocusedProcessId() {
  return getFocusedProcessIdFromHyprland();
}

}  // namespace track::media
