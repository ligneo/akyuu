/**
 * Akyuu
 * Copyright (C) 2010-2026, Eren Okka
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

#include <QRegularExpression>
#include <QString>
#include <optional>
#include <string>
#include <vector>

namespace track::recognition {

enum class Stream {
  Adn,
  Animelab,
  Ann,
  Bilibili,
  Crunchyroll,
  Jellyfin,
  Plex,
  RokuChannel,
  Tubi,
  Veoh,
  Viz,
  Vrv,
  Wakanim,
  Yahoo,
  Youtube,
};

struct StreamData {
  Stream id;
  QString name;
  QString url;
  QRegularExpression urlPattern;
  QRegularExpression titlePattern;
};

const std::vector<StreamData>& streamData();

bool isStreamEnabled(const StreamData& stream);

// Returns the anime title if the page is a known streaming provider. Titles of unknown pages are
// discarded, because any video would be detected otherwise.
std::optional<std::string> titleFromStreamingProvider(const std::string& url,
                                                      const std::string& title);

}  // namespace track::recognition
