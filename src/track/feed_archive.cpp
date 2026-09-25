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

#include "feed_archive.hpp"

#include <QJsonArray>

#include "akyuu/path.hpp"
#include "akyuu/settings.hpp"

namespace track {

using namespace Qt::StringLiterals;

void Archive::init() {
  titles_.clear();

  for (const auto& value : value("titles").toJsonArray()) {
    titles_.append(value.toString());
  }
}

bool Archive::contains(const QString& title) const {
  return titles_.contains(title);
}

void Archive::add(const QString& title) {
  if (title.isEmpty() || contains(title)) return;

  titles_.append(title);
  save();
}

void Archive::clear() {
  titles_.clear();
  save();
}

int Archive::size() const {
  return titles_.size();
}

QString Archive::fileName() const {
  return u"%1/torrents.json"_s.arg(QString::fromStdString(akyuu::get_data_path()));
}

void Archive::save() const {
  // Only the most recent titles are kept, as in v1: an archive that grows forever would be read
  // and written in full on every download.
  const auto maxCount = akyuu::settings.torrentArchiveMaxCount();
  const auto titles =
      maxCount > 0 && titles_.size() > maxCount ? titles_.mid(titles_.size() - maxCount) : titles_;

  setValue("titles", QJsonArray::fromStringList(titles));
}

}  // namespace track
