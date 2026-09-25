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

#include <QCoreApplication>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <map>
#include <unordered_map>

namespace track {

// Keeps track of which episodes are present in the library folders. The data is not stored, it is
// rebuilt by scanning, as in v1.
class Library final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(Library)

public:
  Library(QObject* parent);
  ~Library() override = default;

  void scan();
  void scan(const int animeId);
  void applyWatchSettings();

  bool hasScanned() const;
  int availableEpisodeCount(const int animeId) const;
  int lastAvailableEpisode(const int animeId) const;
  bool isEpisodeAvailable(const int animeId, const int number) const;
  QString episodePath(const int animeId, const int number) const;

signals:
  void availabilityChanged();
  void scanCompleted(const int animeCount, const int episodeCount);

private:
  void onDirectoryChanged(const QString& path);
  static void walk(const QString& folder,
                   const std::function<void(int animeId, int number, const QString& path)>& found);

  // Anime ID to episode number to file path
  std::unordered_map<int, std::map<int, QString>> episodes_;
  QFileSystemWatcher* watcher_ = nullptr;
  QTimer* rescanTimer_ = nullptr;
  bool scanned_ = false;
};

inline Library* library() {
  static auto instance = new Library(qApp);
  return instance;
}

}  // namespace track
