/*
 * Akyuu, an anime tracker application for Windows and Linux.
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

namespace release {

void packageWindows(const QString& root, const QString& build, const QString& qt,
                    const QString& licenses, const QString& output);
void testWindows(const QString& installer, const QString& probe);
void prepareWindowsLicenses(const QString& output);

}  // namespace release
