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

#include "media.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>

#include "akyuu/settings.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "track/episode.hpp"
#include "track/media_player.hpp"
#include "track/media_stream.hpp"
#ifdef Q_OS_LINUX
#include "track/media_focus.hpp"
#include "track/media_mpris.hpp"
#endif
#include "track/recognition.hpp"

namespace track::media {

namespace {

struct MediaFields {
  std::string file;
  std::string title;
  std::string url;
};

anisthesia::Media flattenMedia(const anisthesia::Result& result) {
  anisthesia::Media media;

  for (const auto& item : result.media) {
    media.information.append_range(item.information);
  }

  return media;
}

bool isVideoFile(const std::string& path) {
  const auto fileName = QFileInfo{QString::fromStdString(path)}.fileName();
  return track::recognition::isVideoFile(track::recognition::parse(fileName.toStdString()));
}

MediaFields extractMediaFields(const anisthesia::Media& media) {
  MediaFields fields;

  for (const auto& information : media.information) {
    switch (information.type) {
      case anisthesia::MediaInfoType::File:
        if (fields.file.empty()) fields.file = information.value;
        break;
      case anisthesia::MediaInfoType::Url:
        fields.url = information.value;
        break;
      case anisthesia::MediaInfoType::Title:
        if (fields.title.empty()) fields.title = information.value;
        break;
      case anisthesia::MediaInfoType::Unknown:
        // Always prefer window title over page title reported via UI Automation.
        fields.title = information.value;
        break;
      case anisthesia::MediaInfoType::Tab:
        break;
    }
  }

  return fields;
}

std::optional<Episode> resolveEpisode(const MediaFields& fields) {
  if (!fields.file.empty()) {
    const QFileInfo fileInfo{QString::fromStdString(fields.file)};
    auto episode = track::recognition::parseFileInfo(fileInfo);

    if (!track::recognition::isVideoFile(episode)) return std::nullopt;

    return episode;
  }

  auto value = fields.title;

  if (value.empty()) return std::nullopt;

  if (!fields.url.empty()) {
    const auto title = track::recognition::titleFromStreamingProvider(fields.url, value);
    if (!title) return std::nullopt;

    value = *title;
  }

  return track::recognition::parse(value);
}

}  // namespace

Detection::Detection(QObject* parent) : QObject(parent) {
  pollTimer_ = new QTimer(this);
  connect(pollTimer_, &QTimer::timeout, this, &Detection::poll);
}

const std::optional<Episode> Detection::getCurrentEpisode() const {
  return currentEpisode_;
}

const std::optional<std::string> Detection::getCurrentFile() const {
  if (!currentMedia_) return std::nullopt;

  auto file = extractMediaFields(*currentMedia_).file;
  if (file.empty()) return std::nullopt;

  return file;
}

const std::optional<Detection::media_t> Detection::getCurrentMedia() const {
  return currentMedia_;
}

const std::optional<Detection::player_t> Detection::getCurrentPlayer() const {
  return currentPlayer_;
}

bool Detection::init() {
  if (!parsePlayersData(players_)) {
    return false;
  }

  setEnabled(akyuu::settings.mediaDetectionEnabled());

  return true;
}

// v1's `program/general/enablerecognition`: detection can be turned off without quitting.
void Detection::setEnabled(const bool enabled) {
  if (enabled) {
    pollTimer_->start(akyuu::settings.mediaDetectionInterval());
  } else {
    pollTimer_->stop();
    reset();
  }
  emit enabledChanged(enabled);
}

void Detection::poll() {
  const auto players = getEnabledPlayers(players_);

  static const auto media_proc = [](const anisthesia::MediaInfo& info) {
    // Players may keep other files open as well (e.g. caches, databases)
    if (info.type == anisthesia::MediaInfoType::File) return isVideoFile(info.value);
    return true;
  };

  std::vector<anisthesia::Result> results;
  if (!anisthesia::GetResults(players, media_proc, results)) {
    results.clear();
  }
#ifdef Q_OS_LINUX
  std::ranges::move(getMprisResults(players), std::back_inserter(results));
#endif
  if (results.empty()) {
    reset();
    return;
  }

  const auto resultIt = std::ranges::find(results, currentPlayerId_, &anisthesia::Result::id);
  const auto& result = resultIt != results.end() ? *resultIt : results.front();

  currentPlayer_ = result.player;
  currentMedia_ = flattenMedia(result);
  currentPlayerId_ = result.id;

  auto episode = resolveEpisode(extractMediaFields(*currentMedia_));
  if (!episode) {
    reset();
    return;
  }

  const auto animeId = track::recognition::identify(*episode);
  episode->setAnimeId(animeId);

  if (hasEpisodeChanged(*episode)) {
    currentEpisode_ = episode;
    emit currentEpisodeChanged(episode);
  }
}

bool Detection::isPlayerFocused() const {
#ifdef Q_OS_WINDOWS
  const auto window = reinterpret_cast<std::uintptr_t>(GetForegroundWindow());
  return currentPlayerId_.window && currentPlayerId_.window == window;
#elif defined(Q_OS_LINUX)
  const auto processId = getFocusedProcessId();
  return !processId || *processId == currentPlayerId_.process;  // assume focused if we can't tell
#else
  return true;  // no way to tell yet
#endif
}

bool Detection::isMediaIdentified() const {
  return currentEpisode_.has_value() && currentEpisode_->animeId() != anime::kUnknownId;
}

void Detection::setCurrentEpisodeAnimeId(int animeId) {
  if (!currentEpisode_) return;

  currentEpisode_->setAnimeId(animeId);
  emit currentEpisodeChanged(currentEpisode_);
}

void Detection::reset() {
  currentPlayer_.reset();
  currentMedia_.reset();
  currentPlayerId_ = {};

  if (currentEpisode_) {
    currentEpisode_.reset();
    emit currentEpisodeChanged(std::nullopt);
  }
}

bool Detection::hasEpisodeChanged(const Episode& episode) const {
  if (!currentEpisode_) return true;
  if (currentEpisode_->animeId() != episode.animeId()) return true;

  return currentEpisode_->elements(anitomy::ElementKind::Episode) !=
         episode.elements(anitomy::ElementKind::Episode);
}

}  // namespace track::media
