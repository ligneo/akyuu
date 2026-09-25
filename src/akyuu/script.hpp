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

#include "track/episode.hpp"

namespace taiga {

// Fills in the variables a format string may name, then evaluates its functions. Values are
// percent-encoded when the result is going into a request, and left alone otherwise.
QString replaceVariables(const QString& format, const track::Episode& episode,
                         const bool urlEncode = false);

}  // namespace taiga
