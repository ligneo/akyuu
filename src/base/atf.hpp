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

#include <QString>
#include <map>
#include <optional>

// @TODO: Replace with https://github.com/erengy/atf when available

namespace atf {

// A field without a value removes its variable from the string, rather than leaving it empty.
using field_map_t = std::map<QString, std::optional<QString>>;

// Substitutes %variables% first, then evaluates the $functions() over the result. Values are
// escaped as they are substituted, so a title that contains a dollar sign or a comma cannot turn
// into syntax of its own.
QString replace(QString str, const field_map_t& fields);

}  // namespace atf
