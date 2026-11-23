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

#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QPixmap>
#include <QRestAccessManager>
#include <QSet>
#include <QString>

namespace gui {

class ImageProvider final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ImageProvider)

public:
  ImageProvider();

  void init();

  void fetchPoster(const int id, const bool revalidate = false);
  QPixmap loadPoster(const int id);
  void reloadPoster(const int id);

  // The pictures of the account, which are kept on disk and checked for a new version once a day
  QPixmap loadUserImage(const QString& url);

  QString cachePath() const;
  void clearCache();

signals:
  void posterChanged(const int id);
  void userImageChanged(const QString& url);

private:
  QString fileName(const int id) const;
  bool isStale(const int id) const;
  bool canRetry(const int id) const;
  void retryAfter(const int id);
  void fetchUserImage(const QString& url, const bool revalidate);
  QString userImageFileName(const QString& url) const;

  QRestAccessManager* m_manager = nullptr;
  QSet<int> m_loading;
  QMap<int, QDateTime> m_retryAfter;
  QSet<QString> m_userImagesChecked;
};

inline ImageProvider imageProvider;

}  // namespace gui
