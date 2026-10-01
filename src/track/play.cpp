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

#include "play.hpp"

#include <QDesktopServices>
#include <QProcess>
#include <QRandomGenerator>
#include <QUrl>
#include <algorithm>
#include <vector>

#include "akyuu/settings.hpp"
#include "base/log.hpp"
#include "media/anime_db.hpp"
#include "media/anime_history.hpp"
#include "media/anime_list.hpp"
#include "track/library.hpp"
#include "track/scanner.hpp"

namespace track {

namespace {

// v1's `recognition/mediaplayers/launchpath`: episodes open in a chosen player rather than the
// default application for the file type. The command may carry arguments, e.g. `mpv --fs`.
bool openEpisodeFile(const QString& path) {
  const auto command = QString::fromStdString(akyuu::settings.libraryMediaPlayerPath());
  auto arguments = QProcess::splitCommand(command);

  if (arguments.isEmpty()) {
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  }

  const auto program = arguments.takeFirst();
  arguments.append(path);

  return QProcess::startDetached(program, arguments);
}

}  // namespace

bool playEpisode(int animeId, int number) {
  // The library already knows where the episodes are, so the folders only have to be walked when
  // it has nothing for this one.
  if (const auto path = library()->episodePath(animeId, number); !path.isEmpty()) {
    qDebug() << "Found file:" << path;
    return openEpisodeFile(path);
  }

  for (const auto& folder : akyuu::settings.libraryFolders()) {
    const auto episodePath = findEpisode(QString::fromStdString(folder), animeId, number);
    if (episodePath) {
      qDebug() << "Found file:" << *episodePath;
      return openEpisodeFile(*episodePath);
    }
  }

  qDebug() << "Not found:" << animeId << "episode" << number;

  return false;
}

namespace {

// Anime the user has moved on from are not what "play next" means, as in v1.
bool isWorthPlaying(const int animeId) {
  const auto entry = anime::db.entry(animeId);

  if (!entry) return false;

  switch (entry->status) {
    case anime::list::Status::NotInList:
    case anime::list::Status::Completed:
    case anime::list::Status::Dropped:
      return false;
    default:
      return true;
  }
}

}  // namespace

std::optional<int> nextEpisodeNumber(int animeId) {
  const auto item = anime::db.item(animeId);
  if (!item) return std::nullopt;

  const auto entry = anime::db.entry(animeId);
  const int watched_episodes = entry ? entry->watched_episodes : 0;

  int number = watched_episodes + 1;

  // Replay first episode for completed anime.
  if (item->episode_count > 0 && number > item->episode_count) {
    number = 1;
  }

  return number;
}

std::optional<int> randomEpisodeNumber(int animeId) {
  const auto item = anime::db.item(animeId);
  if (!item) return std::nullopt;

  const auto entry = anime::db.entry(animeId);
  const int watched_episodes = entry ? entry->watched_episodes : 0;

  int max_episode = item->episode_count;

  // Avoid spoiling unwatched episodes, unless the series is completed.
  // `watched_episodes < episode_count` can be true if rewatching.
  const bool completed = entry && entry->status == anime::list::Status::Completed;
  if (!completed) {
    max_episode = watched_episodes + 1;
  }

  // Clamp to the known episode count, if any.
  if (item->episode_count > 0) {
    max_episode = std::min(max_episode, item->episode_count);
  }

  if (max_episode < 1) return std::nullopt;

  return QRandomGenerator::global()->bounded(1, max_episode + 1);
}

// v1 walks the queue and then the history, because an episode it has just seen may still be
// waiting to be sent. In v2 both are written at the same moment (`anime::list::update()`), so the
// history alone is enough.
bool playNextEpisodeOfLastWatchedAnime() {
  const anime::HistoryItem* latest = nullptr;

  for (const auto& item : anime::history.items()) {
    if (item.episode <= 0) continue;
    if (!isWorthPlaying(item.anime_id)) continue;
    if (!latest || item.time > latest->time) latest = &item;
  }

  if (!latest) {
    qDebug() << "No recently watched anime to continue.";
    return false;
  }

  const auto number = nextEpisodeNumber(latest->anime_id);

  return number && playEpisode(latest->anime_id, *number);
}

bool playRandomAnime() {
  // v1 rescans if its data is more than two minutes old. Scanning the user's folders was measured
  // at 51 ms, so it is simply done every time.
  library()->scan();

  std::vector<int> candidates;

  for (const auto id : anime::db.items().keys()) {
    if (!isWorthPlaying(id)) continue;

    const auto number = nextEpisodeNumber(id);
    if (!number) continue;

    // Without this, the action would keep landing on anime whose next episode is not on disk.
    if (!library()->isEpisodeAvailable(id, *number)) continue;

    candidates.push_back(id);
  }

  if (candidates.empty()) {
    qDebug() << "No anime with an available next episode.";
    return false;
  }

  std::ranges::shuffle(candidates, *QRandomGenerator::global());

  for (const auto id : candidates) {
    const auto number = nextEpisodeNumber(id);
    if (number && playEpisode(id, *number)) return true;
  }

  return false;
}

}  // namespace track
