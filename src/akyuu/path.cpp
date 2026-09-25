/**
 * Taiga
 * Copyright (C) 2010-2024, Eren Okka
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

#include "path.hpp"

#include <QCoreApplication>
#include <QStandardPaths>
#include <filesystem>
#include <format>

#include "akyuu/config.h"

namespace akyuu {

// Returns current path in portable mode, AppData location otherwise
std::string get_data_path() {
#ifdef AKYUU_PORTABLE
  return std::format("{}/data", QCoreApplication::applicationDirPath().toStdString());
#else
  const auto location = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
  return std::format("{}/data", location.first().toStdString());
#endif
}

// Akyuu started out as a fork of Taiga, which kept its data in `erengy/taiga` under the same
// location. The first run starts from a copy of that folder, so the list, history and accounts
// carry over. The original is left in place: Taiga can still be run side by side with it.
void migrate_taiga_data() {
#ifndef AKYUU_PORTABLE
  namespace fs = std::filesystem;

  const fs::path target = get_data_path();
  if (fs::exists(target)) return;

  const auto location = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  const fs::path source = std::format("{}/erengy/taiga/data", location.toStdString());
  if (!fs::is_directory(source)) return;

  std::error_code error;
  fs::create_directories(target.parent_path(), error);
  fs::copy(source, target, fs::copy_options::recursive, error);
#endif
}

}  // namespace akyuu
