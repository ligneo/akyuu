/**
 * Akyuu
 * Copyright (C) 2010-2024, Eren Okka
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

#include "settings.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonParseError>

namespace {

QSettings::Format jsonSettingsFormat() {
  static const auto read = [](QIODevice& device, QSettings::SettingsMap& map) {
    QJsonParseError error;
    map = QJsonDocument::fromJson(device.readAll(), &error).toVariant().toMap();
    return error.error == QJsonParseError::NoError;
  };

  static const auto write = [](QIODevice& device, const QSettings::SettingsMap& map) {
    const auto json = QJsonDocument::fromVariant(map).toJson();
    return device.write(json) == json.size();
  };

  static const auto format = QSettings::registerFormat("json", read, write);
  return format;
}

}  // namespace

namespace base {

QVariant Settings::value(QAnyStringView key) const {
  return value(key, {});
}

QVariant Settings::value(QAnyStringView key, const QVariant& defaultValue) const {
  if (!prepareFile(false)) return defaultValue;
  return settings().value(key, defaultValue);
}

void Settings::setValue(QAnyStringView key, const QVariant& value) const {
  if (!prepareFile(true)) return;

  auto store = settings();
  store.setValue(key, value);
  if (isPrivate()) {
    store.sync();
    if (store.status() != QSettings::NoError) {
      qWarning() << "Could not save private settings:" << fileName();
    }
  }
}

void Settings::setValue(QAnyStringView key, const std::string_view value) const {
  setValue(key, QString::fromUtf8(value));
}

bool Settings::prepareFile(const bool create) const {
  if (!isPrivate()) return true;

  const auto path = fileName();
  const auto permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

  const QFileInfo fileInfo{path};
  if (fileInfo.exists()) {
    if (!fileInfo.isFile()) {
      qWarning() << "Private settings path is not a regular file:" << path;
      return false;
    }
    if (QFile::setPermissions(path, permissions)) return true;
    qWarning() << "Could not restrict private settings permissions:" << path;
    return false;
  }
  if (!create) return true;

  if (!QDir{}.mkpath(QFileInfo{path}.absolutePath())) {
    qWarning() << "Could not create private settings directory:" << path;
    return false;
  }

  // Create the destination before QSettings writes it, so atomic replacements inherit its mode.
  QFile file{path};
  if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly, permissions)) {
    // Another writer may have created it after the existence check.
    if (QFileInfo{path}.isFile() && QFile::setPermissions(path, permissions)) return true;
    qWarning() << "Could not create private settings:" << path << file.errorString();
    return false;
  }
  if (file.write("{}") == 2 && file.flush()) return true;

  qWarning() << "Could not initialize private settings:" << path << file.errorString();
  return false;
}

QSettings Settings::settings() const {
  return QSettings(fileName(), jsonSettingsFormat());
}

}  // namespace base
