/*
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QString>

namespace release {

void prepareLinuxSdk(const QString& sdk);
void buildLinux(const QString& root, const QString& output);
void packageAppImage(const QString& root, const QString& build, const QString& appdir,
                     const QString& licenses, const QString& output);
void packageLinux(const QString& root, const QString& format, const QString& bundle,
                  const QString& releaseVersion, const QString& output);
void packageArch(const QString& root, const QString& archive, const QString& output);
void testLinux(const QString& format, const QString& packages);

}  // namespace release
