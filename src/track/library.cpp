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

#include "library.hpp"

#include <QDirIterator>
#include <chrono>
#include <functional>

#include "akyuu/settings.hpp"
#include "base/log.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "track/episode.hpp"
#include "track/recognition.hpp"

namespace track {

Library::Library(QObject* parent) : QObject(parent) {}

// Every video file under the folder that belongs to a known anime, with its episode number.
void Library::walk(const QString& folder,
                   const std::function<void(int animeId, int number, const QString& path)>& found) {
  QDirIterator it{folder, QDir::Files, QDirIterator::Subdirectories};

  while (it.hasNext()) {
    const auto info = it.nextFileInfo();

    auto episode = recognition::parseFileInfo(info);

    if (!recognition::isVideoFile(episode)) continue;

    // v1 skips anything smaller than the threshold, which keeps samples and stubs out.
    if (const auto minimum = akyuu::settings.libraryMinimumFileSize();
        minimum > 0 && info.size() < minimum) {
      continue;
    }

    const auto animeId = recognition::identify(episode);

    if (animeId == anime::kUnknownId) continue;

    const auto range = episode.episodeNumberRange();

    // A file without an episode number is taken to be the first episode, as in v1.
    const int number = range ? range->second : 1;

    const auto item = anime::db.item(animeId);

    // Anything beyond the known episode count is a misparse rather than a new episode.
    if (item && item->episode_count > 0 && number > item->episode_count) continue;

    found(animeId, number, info.filePath());
  }
}

void Library::scan() {
  decltype(episodes_) episodes;
  int episodeCount = 0;

  for (const auto& folder : akyuu::settings.libraryFolders()) {
    walk(QString::fromStdString(folder),
         [&episodes, &episodeCount](const int animeId, const int number, const QString& path) {
           if (episodes[animeId].try_emplace(number, path).second) ++episodeCount;
         });
  }

  episodes_ = std::move(episodes);
  scanned_ = true;

  qInfo() << "Found" << episodeCount << "episodes of" << episodes_.size() << "anime";

  emit availabilityChanged();
  emit scanCompleted(static_cast<int>(episodes_.size()), episodeCount);
}

// v1's `ScanEpisodes()`: one anime, starting with the folder set for it, which may lie outside the
// library folders. The library folders are searched only when that finds nothing, as in v1.
void Library::scan(const int animeId) {
  QStringList folders;
  if (const auto settings = anime::db.settings(animeId); settings && !settings->folder.empty()) {
    folders.append(QString::fromStdString(settings->folder));
  }
  const auto libraryFolders = akyuu::settings.libraryFolders();
  const auto first = folders.size();
  for (const auto& folder : libraryFolders) folders.append(QString::fromStdString(folder));

  std::map<int, QString> episodes;

  for (qsizetype i = 0; i < folders.size(); ++i) {
    if (i == first && first > 0 && !episodes.empty()) break;
    walk(folders.at(i), [animeId, &episodes](const int id, const int number, const QString& path) {
      if (id == animeId) episodes.try_emplace(number, path);
    });
  }

  const auto episodeCount = static_cast<int>(episodes.size());

  if (episodes.empty()) {
    episodes_.erase(animeId);
  } else {
    episodes_[animeId] = std::move(episodes);
  }

  qInfo() << "Found" << episodeCount << "episodes of anime" << animeId;

  emit availabilityChanged();
  emit scanCompleted(episodeCount > 0 ? 1 : 0, episodeCount);
}

int Library::availableEpisodeCount(const int animeId) const {
  const auto it = episodes_.find(animeId);
  return it != episodes_.end() ? static_cast<int>(it->second.size()) : 0;
}

// Until the library folders have been scanned once, nothing is known to be missing either.
bool Library::hasScanned() const {
  return scanned_;
}

int Library::lastAvailableEpisode(const int animeId) const {
  const auto it = episodes_.find(animeId);
  return it != episodes_.end() && !it->second.empty() ? it->second.rbegin()->first : 0;
}

bool Library::isEpisodeAvailable(const int animeId, const int number) const {
  const auto it = episodes_.find(animeId);
  return it != episodes_.end() && it->second.contains(number);
}

QString Library::episodePath(const int animeId, const int number) const {
  const auto it = episodes_.find(animeId);

  if (it == episodes_.end()) return {};

  const auto episode = it->second.find(number);

  return episode != it->second.end() ? episode->second : QString{};
}

// v1 reacts to each file event; Qt only says which directory changed, so the whole library is
// scanned again. A short delay keeps a burst of writes from starting several scans.
void Library::applyWatchSettings() {
  if (!watcher_) {
    watcher_ = new QFileSystemWatcher(this);
    rescanTimer_ = new QTimer(this);
    rescanTimer_->setSingleShot(true);
    rescanTimer_->setInterval(std::chrono::seconds{3});
    connect(rescanTimer_, &QTimer::timeout, this, qOverload<>(&Library::scan));
    connect(watcher_, &QFileSystemWatcher::directoryChanged, this, &Library::onDirectoryChanged);
  }

  if (!watcher_->directories().isEmpty()) watcher_->removePaths(watcher_->directories());

  if (!akyuu::settings.libraryWatchFolders()) return;

  QStringList paths;

  for (const auto& folder : akyuu::settings.libraryFolders()) {
    const auto root = QString::fromStdString(folder);
    if (!QDir{root}.exists()) continue;
    paths.append(root);
    // Subfolders have to be watched one by one; a watch does not reach into them.
    QDirIterator it{root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories};
    while (it.hasNext()) paths.append(it.next());
  }

  if (!paths.isEmpty()) watcher_->addPaths(paths);
}

void Library::onDirectoryChanged(const QString&) {
  rescanTimer_->start();
}

}  // namespace track
