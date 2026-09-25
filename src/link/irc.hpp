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

#pragma once

#include <QString>
#include <QStringList>

#include "track/episode.hpp"

namespace link::irc {

// v1 tells a running mIRC what to write, over DDE. Konversation is the client this speaks to
// instead: it is driven over D-Bus, which Akyuu already uses for media detection.
bool isRunning();

// The connections the client has open, and the channels joined on one of them. Both are empty
// unless the client is running, and neither starts it.
QStringList connections();
QStringList joinedChannels(const QString& connection);

// `force` shares even when the channel is turned off: v1 does that when asked from the menu.
void announce(const track::Episode& episode, const bool force = false);

}  // namespace link::irc
