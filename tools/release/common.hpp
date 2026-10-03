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
#include <QString>
#include <QStringList>

namespace release {
void require(bool condition, const QString& message);
QByteArray readFile(const QString& path);
void writeFile(const QString& path, const QByteArray& data);
QByteArray run(const QString& program, const QStringList& arguments, const QString& directory = {},
               int timeout = 300000);
void call(const QString& program, const QStringList& arguments, const QString& directory = {},
          int timeout = 300000);
void validateVersion(const QString& value);
QString version(const QString& root);
QByteArray sha256(const QString& path);
void copyTree(const QString& source, const QString& destination);
int commonTests();
void sourceBundle(const QString& root, const QString& output);
void bumpVersion(const QString& root, const QString& next);
void createReleaseDraft(const QString& root, const QString& tag, const QString& packages);
}  // namespace release
