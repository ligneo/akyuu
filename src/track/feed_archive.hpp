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
#include <QStringList>

#include "base/settings.hpp"

namespace track {

// Titles that have already been downloaded or thrown away, so that the next check does not offer
// them again. v1 calls this the torrent archive and keeps it in its own file; so does this.
class Archive final : public base::Settings {
public:
  void init();

  bool contains(const QString& title) const;
  void add(const QString& title);
  void clear();
  int size() const;

private:
  QString fileName() const override;
  void save() const;

  QStringList titles_;
};

inline Archive archive;

}  // namespace track
