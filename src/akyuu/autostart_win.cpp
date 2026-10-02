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

#include "autostart.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include "akyuu/settings.hpp"

namespace akyuu {

void applyAutoStart() {
  QSettings startup{QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                    QSettings::NativeFormat};
  if (settings.appAutoStart()) {
    startup.setValue(QStringLiteral("Akyuu"),
                     QStringLiteral("\"%1\" --minimized")
                         .arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
  } else {
    startup.remove(QStringLiteral("Akyuu"));
  }
}

}  // namespace akyuu
