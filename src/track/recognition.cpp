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

#include "recognition.hpp"

#include <QFileInfo>
#include <algorithm>
#include <anitomy.hpp>
#include <format>
#include <ranges>
#include <vector>

#include "akyuu/settings.hpp"
#include "base/string.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "track/episode.hpp"
#include "track/recognition_cache.hpp"
#include "track/recognition_normalize.hpp"
#include "track/recognition_path.hpp"
#include "track/recognition_relations.hpp"
#include "track/recognition_validate.hpp"

namespace track::recognition {

namespace {

QString formatEpisodeRange(const std::pair<int, int>& range) {
  if (range.second > range.first) return u"%1-%2"_s.arg(range.first).arg(range.second);
  return QString::number(range.first);
}

int identifyByTitle(Episode& episode, const std::string& normalizedTitle) {
  std::vector<Cache::Data::Match> matches;

  if (const auto data = cache()->find(normalizedTitle)) {
    matches.append_range(data->matches | std::views::values | std::ranges::to<std::vector>());
  }

  std::ranges::sort(matches, std::ranges::greater{}, &Cache::Data::Match::weight);

  for (const auto& match : matches) {
    const auto item = anime::db.item(match.id);
    if (!item) continue;

    if (!isValidEpisodeType(episode)) continue;

    if (!isValidEpisodeNumber(episode, *item)) {
      const auto range = episode.episodeNumberRange();
      if (!range) continue;

      const auto redirect = findRedirection(match.id, *range);
      if (!redirect) continue;

      qDebug() << u"Redirection: %1:%2 -> %3:%4"_s.arg(match.id)
                      .arg(formatEpisodeRange(*range))
                      .arg(redirect->id)
                      .arg(formatEpisodeRange(redirect->episode_range));
      episode.setEpisodeNumberRange(redirect->episode_range);
      return redirect->id;
    }

    return match.id;
  }

  return anime::kUnknownId;
}

// Sequels are separate entries with titles of their own (e.g. "Sousou no Frieren 2nd Season"),
// while file names only have the season number. Appending the season to the title is enough to
// tell them apart, because normalization reduces both forms to the same number.
std::vector<std::string> titleCandidates(const Episode& episode) {
  const auto title = episode.element(anitomy::ElementKind::Title);
  const auto season = toInt(episode.element(anitomy::ElementKind::Season));

  // Falling back to the bare title would match the first season instead, which is worse than not
  // recognizing the episode at all, because the list would be updated for the wrong entry.
  if (season > 1) {
    return {normalize(std::format("{} Season {}", title, season))};
  }

  return {normalize(title)};
}

// A fractional episode number (e.g. `7.5`) almost always belongs to a recap or special that is
// listed as a separate entry. Taking it as episode 7 would update the wrong entry, so the number is
// moved into the title instead, as v1 did.
void moveFractionalNumberToTitle(Episode& episode) {
  const auto number = episode.element(anitomy::ElementKind::Episode);

  if (!number.contains('.')) return;

  const auto title = episode.element(anitomy::ElementKind::Title);
  episode.removeElements(anitomy::ElementKind::Title);
  episode.addElement(anitomy::ElementKind::Title, std::format("{} Episode {}", title, number));
  episode.removeElements(anitomy::ElementKind::Episode);
}

}  // namespace

Episode parse(std::string_view input, const anitomy::Options options) {
  Episode episode;

  // Strings the user wants ignored are removed first, because Anitomy has no option for them.
  std::string cleaned{input};
  for (const auto& ignored : akyuu::settings.recognitionIgnoredStrings()) {
    if (ignored.empty()) continue;
    for (auto pos = cleaned.find(ignored); pos != std::string::npos;
         pos = cleaned.find(ignored, pos)) {
      cleaned.erase(pos, ignored.length());
    }
  }

  auto elements = anitomy::parse(cleaned, options);
  episode.setElements(elements);

  return episode;
}

Episode parseFileInfo(const QFileInfo& info, const anitomy::Options options) {
  const auto fileName = info.fileName().toStdString();

  Episode episode = track::recognition::parse(fileName, options);

  if (!episode.contains(anitomy::ElementKind::Title) &&
      akyuu::settings.recognitionLookupParentDirectories()) {
    const auto parsed = parseParentDirectories(info);
    if (!parsed.title.empty()) {
      episode.addElement(anitomy::ElementKind::Title, parsed.title);
    }
    if (!parsed.season.empty() && !episode.contains(anitomy::ElementKind::Season)) {
      episode.addElement(anitomy::ElementKind::Season, parsed.season);
    }
  }

  return episode;
}

int identify(Episode& episode) {
  cache()->init();

  moveFractionalNumberToTitle(episode);

  for (const auto& title : titleCandidates(episode)) {
    if (const auto id = identifyByTitle(episode, title); id != anime::kUnknownId) return id;
  }

  return anime::kUnknownId;
}

bool isVideoFile(const Episode& episode) {
  // This relies on Anitomy tagging `FileExtension` only for video
  // container extensions.
  return episode.contains(anitomy::ElementKind::FileExtension);
}

}  // namespace track::recognition
