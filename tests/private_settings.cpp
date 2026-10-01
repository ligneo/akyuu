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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

#include "base/settings.hpp"

class PrivateSettings final : public base::Settings {
public:
  explicit PrivateSettings(const QString& path) : path_(path) {}
  QVariant read(const QString& key) const { return value(key, QStringLiteral("unavailable")); }
  void write(const QString& key, const QString& value) const { setValue(key, value); }

private:
  QString fileName() const override { return path_; }
  bool isPrivate() const override { return true; }
  QString path_;
};

void require(const bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

QByteArray readFile(const QString& path) {
  QFile file{path};
  require(file.open(QIODevice::ReadOnly), "Could not read test fixture");
  return file.readAll();
}

void requirePrivate(const QString& path) {
#ifdef Q_OS_UNIX
  struct stat info {};
  require(::stat(QFile::encodeName(path).constData(), &info) == 0, "Missing private file");
  require((info.st_mode & 0777) == 0600, "Private settings do not have mode 0600");
#else
  require(QFile::exists(path), "Missing private file");
#endif
}

int main(int argc, char** argv) {
  QCoreApplication application{argc, argv};
  QTemporaryDir directory;
  require(directory.isValid(), "Could not create test directory");
#ifdef Q_OS_UNIX
  const auto oldMask = ::umask(0);
#endif

  const auto newPath = directory.filePath(QStringLiteral("nested/accounts.json"));
  PrivateSettings created{newPath};
  require(created.read(QStringLiteral("token")) == QStringLiteral("unavailable"),
          "A missing account file should return the default");
  require(!QFile::exists(newPath), "Reading should not create an empty account file");
  created.write(QStringLiteral("token"), QStringLiteral("fake-token"));
  requirePrivate(newPath);
  require(created.read(QStringLiteral("token")) == QStringLiteral("fake-token"),
          "New private settings were not saved");

  const auto existingPath = directory.filePath(QStringLiteral("existing.json"));
  const QByteArray original = "{\n  \"token\": \"fake-old-token\", \"other\": 123\n}\n";
  {
    QFile file{existingPath};
    require(file.open(QIODevice::WriteOnly), "Could not create public fixture");
    require(file.write(original) == original.size(), "Could not write public fixture");
  }
  require(QFile::setPermissions(existingPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                QFileDevice::ReadGroup | QFileDevice::ReadOther),
          "Could not set fixture mode 0644");
  PrivateSettings existing{existingPath};
  require(existing.read(QStringLiteral("token")) == QStringLiteral("fake-old-token"),
          "Existing token was not preserved");
  requirePrivate(existingPath);
  require(readFile(existingPath) == original, "Restricting permissions rewrote existing content");
  for (int i = 0; i < 3; ++i) {
#ifdef Q_OS_UNIX
    struct stat before {};
    require(::stat(QFile::encodeName(existingPath).constData(), &before) == 0,
            "Could not inspect existing fixture");
#endif
    existing.write(QStringLiteral("refresh"), QString::number(i));
    requirePrivate(existingPath);
#ifdef Q_OS_UNIX
    struct stat after {};
    require(::stat(QFile::encodeName(existingPath).constData(), &after) == 0,
            "Could not inspect replaced fixture");
    require(before.st_ino != after.st_ino, "The account update did not replace the file atomically");
#endif
    const auto object = QJsonDocument::fromJson(readFile(existingPath)).object();
    require(object.value(QStringLiteral("token")).toString() == QStringLiteral("fake-old-token"),
            "Atomic update lost the token");
    require(object.value(QStringLiteral("other")).toInt() == 123,
            "Atomic update lost unrelated account data");
  }

  const auto directoryPath = directory.filePath(QStringLiteral("directory.json"));
  require(QDir{}.mkdir(directoryPath), "Could not create directory fixture");
  const auto directoryPermissions = QFile::permissions(directoryPath);
  PrivateSettings notAFile{directoryPath};
  notAFile.write(QStringLiteral("token"), QStringLiteral("must-not-be-written"));
  require(notAFile.read(QStringLiteral("token")) == QStringLiteral("unavailable"),
          "A directory destination should return the default");
  require(QFile::permissions(directoryPath) == directoryPermissions,
          "Rejecting a directory changed its permissions");

  PrivateSettings blocked{existingPath + QStringLiteral("/accounts.json")};
  blocked.write(QStringLiteral("token"), QStringLiteral("must-not-be-written"));
  require(blocked.read(QStringLiteral("token")) == QStringLiteral("unavailable"),
          "A blocked destination should return the default");
  require(!readFile(existingPath).contains("must-not-be-written"),
          "A failed creation changed existing content");
#ifdef Q_OS_LINUX
  PrivateSettings unrestrictable{QStringLiteral("/proc/self/status")};
  require(unrestrictable.read(QStringLiteral("token")) == QStringLiteral("unavailable"),
          "A permissions failure should return the default");
  unrestrictable.write(QStringLiteral("token"), QStringLiteral("must-not-be-written"));
#endif
#ifdef Q_OS_UNIX
  ::umask(oldMask);
#endif
  std::cout << "Private settings: creation, migration, atomic updates and failures passed\n";
}
