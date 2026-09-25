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

#include "script.hpp"

#include <QFileInfo>
#include <QUrl>

#include "base/atf.hpp"
#include "gui/utils/format.hpp"
#include "media/anime_db.hpp"
#include "media/anime_season.hpp"
#include "media/anime_utils.hpp"
#include "sync/service.hpp"
#include "akyuu/accounts.hpp"
#include "track/media.hpp"
#include "track/update_session.hpp"

namespace taiga {

using namespace Qt::StringLiterals;

namespace {

// v1 leaves a count out of the result when it is not known, rather than writing a zero.
std::optional<QString> optionalNumber(const int value) {
  return value > 0 ? std::optional{QString::number(value)} : std::nullopt;
}

std::optional<QString> optionalElement(const track::Episode& episode,
                                       const anitomy::ElementKind kind) {
  if (!episode.contains(kind)) return std::nullopt;
  return QString::fromStdString(episode.element(kind));
}

QString playStatus() {
  using Phase = track::UpdateState::Phase;

  if (track::updateSession()->state().phase == Phase::Committed) return u"updated"_s;
  if (track::media::detection()->getCurrentEpisode()) return u"playing"_s;

  return u"stopped"_s;
}

}  // namespace

QString replaceVariables(const QString& format, const track::Episode& episode,
                         const bool urlEncode) {
  const auto item = anime::db.item(episode.animeId());
  const auto entry = anime::db.entry(episode.animeId());

  // Every variable v1 understands is registered, so that one it cannot fill disappears from the
  // result instead of being left in it. `%manual%` has no counterpart yet: v2 has no way to
  // announce an episode by hand.
  atf::field_map_t fields{
      {u"animeurl"_s, std::nullopt},   {u"audio"_s, std::nullopt},
      {u"checksum"_s, std::nullopt},   {u"episode"_s, std::nullopt},
      {u"file"_s, std::nullopt},       {u"folder"_s, std::nullopt},
      {u"group"_s, std::nullopt},      {u"id"_s, std::nullopt},
      {u"image"_s, std::nullopt},      {u"manual"_s, std::nullopt},
      {u"name"_s, std::nullopt},       {u"notes"_s, std::nullopt},
      {u"playstatus"_s, std::nullopt}, {u"resolution"_s, std::nullopt},
      {u"rewatching"_s, std::nullopt}, {u"score"_s, std::nullopt},
      {u"season"_s, std::nullopt},     {u"status"_s, std::nullopt},
      {u"title"_s, std::nullopt},      {u"total"_s, std::nullopt},
      {u"user"_s, std::nullopt},       {u"version"_s, std::nullopt},
      {u"video"_s, std::nullopt},      {u"watched"_s, std::nullopt},
  };

  fields[u"title"_s] = item ? QString::fromStdString(anime::preferredTitle(*item))
                            : QString::fromStdString(episode.element(anitomy::ElementKind::Title));

  if (const auto range = episode.episodeNumberRange()) {
    auto number = QString::number(range->second);
    while (number.size() > 1 && number.startsWith(u'0')) number.remove(0, 1);
    fields[u"episode"_s] = number;
  }

  if (item) {
    fields[u"animeurl"_s] = sync::animePageUrl(item->id);
    fields[u"id"_s] = QString::number(item->id);
    fields[u"image"_s] = QString::fromStdString(item->image_url);
    fields[u"season"_s] = gui::formatSeason(anime::Season{item->date_started}, {});
    fields[u"total"_s] = optionalNumber(item->episode_count);
  }

  if (entry) {
    fields[u"notes"_s] = QString::fromStdString(entry->notes);
    fields[u"rewatching"_s] = QString::number(entry->rewatching ? 1 : 0);
    fields[u"score"_s] = optionalNumber(entry->score);
    fields[u"status"_s] = QString::number(static_cast<int>(entry->status));
    fields[u"watched"_s] = optionalNumber(entry->watched_episodes);
  }

  fields[u"audio"_s] = optionalElement(episode, anitomy::ElementKind::AudioTerm);
  fields[u"checksum"_s] = optionalElement(episode, anitomy::ElementKind::FileChecksum);
  fields[u"group"_s] = optionalElement(episode, anitomy::ElementKind::ReleaseGroup);
  fields[u"name"_s] = optionalElement(episode, anitomy::ElementKind::EpisodeTitle);
  fields[u"resolution"_s] = optionalElement(episode, anitomy::ElementKind::VideoResolution);
  fields[u"version"_s] = optionalElement(episode, anitomy::ElementKind::ReleaseVersion);
  fields[u"video"_s] = optionalElement(episode, anitomy::ElementKind::VideoTerm);

  if (const auto file = track::media::detection()->getCurrentFile()) {
    const QFileInfo info{QString::fromStdString(*file)};
    fields[u"file"_s] = info.fileName();
    fields[u"folder"_s] = info.path();
  }

  fields[u"playstatus"_s] = playStatus();

  if (const auto user = taiga::accounts.serviceUsername(
          sync::serviceSlug(sync::currentServiceId()).toStdString());
      !user.empty()) {
    fields[u"user"_s] = QString::fromStdString(user);
  }

  if (urlEncode) {
    for (auto& [name, value] : fields) {
      if (value) value = QString::fromUtf8(QUrl::toPercentEncoding(*value));
    }
  }

  return atf::replace(format, fields);
}

}  // namespace taiga
