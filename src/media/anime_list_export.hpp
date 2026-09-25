/**
 * Akyuu
 * Copyright (C) 2010-2025, Eren Okka
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

#include <QMap>
#include <string>

namespace anime::list {

bool exportAsMarkdown(const std::string& path);
// `malIds` maps the list's IDs to MyAnimeList's, when they are not the same service. Anime without
// one are left out, since MyAnimeList would turn the whole entry down anyway.
bool exportAsXml(const std::string& path, const QMap<int, int>* malIds = nullptr,
                 int* skipped = nullptr);

}  // namespace anime::list
