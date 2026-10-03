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
#include <QByteArray>
#include <QMap>
#include <QString>
namespace release {
struct ArchiveEntry {
  QString name;
  char type;
  qint64 size;
  qint64 offset;
  QString link;
  QByteArray digest;
};
class TarArchive {
public:
  explicit TarArchive(const QString& path);
  const QMap<QString, ArchiveEntry>& entries() const {
    return entries_;
  }
  QByteArray read(const QString& name, qint64 limit = 16777216) const;

private:
  QString path_;
  QMap<QString, ArchiveEntry> entries_;
};
}  // namespace release
