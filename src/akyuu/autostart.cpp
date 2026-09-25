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
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include "akyuu/config.h"
#include "akyuu/settings.hpp"
#include "base/log.hpp"

namespace akyuu {

using namespace Qt::StringLiterals;

namespace {

QString autoStartFileName() {
  const auto config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  return u"%1/autostart/%2.desktop"_s.arg(config).arg(AKYUU_APP_NAME);
}

}  // namespace

void applyAutoStart() {
  const auto path = autoStartFileName();

  if (!settings.appAutoStart()) {
    if (QFile::exists(path)) QFile::remove(path);
    return;
  }

  QDir().mkpath(QFileInfo{path}.path());

  QFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Could not write the autostart entry:" << path;
    return;
  }

  const auto entry =
      u"[Desktop Entry]\nType=Application\nName=%1\nExec=%2\nTerminal=false\n"_s.arg(AKYUU_APP_NAME)
          .arg(QCoreApplication::applicationFilePath());

  file.write(entry.toUtf8());
}

}  // namespace akyuu
