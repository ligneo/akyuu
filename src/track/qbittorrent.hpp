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

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>

class QNetworkReply;

namespace track {

// Where a qBittorrent Web UI is, and how to sign in to it
struct QBittorrentServer {
  QUrl url;
  QString username;
  QString password;
};

// Adds torrents to qBittorrent through its Web UI. Unlike opening a file with it, this works when
// qBittorrent runs without a window or on another computer, and the download folder always
// arrives.
class QBittorrent final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(QBittorrent)

public:
  // An empty error means success
  using callback_t = std::function<void(const QString& error)>;
  using test_callback_t = std::function<void(const QString& version, const QString& error)>;

  explicit QBittorrent(QObject* parent = nullptr);
  ~QBittorrent() = default;

  // The target is a magnet link or the path of a .torrent file. An empty folder or category
  // leaves them to qBittorrent.
  void add(const QBittorrentServer& server, const QString& target, const QString& folder,
           const QString& category, callback_t callback);

  // Signs in and asks for the version, to tell whether the settings work
  void test(const QBittorrentServer& server, test_callback_t callback);

private:
  void add(const QBittorrentServer& server, const QString& target, const QString& folder,
           const QString& category, callback_t callback, bool retry);
  void login(const QBittorrentServer& server, callback_t callback);

  QNetworkAccessManager manager_;
};

}  // namespace track
