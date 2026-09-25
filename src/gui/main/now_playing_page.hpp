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

#include <QWidget>

class QTextBrowser;
class QUrl;

namespace gui {

class PosterWidget;

// v1's Now Playing page: the anime being watched, or, when nothing is, where to pick up from.
class NowPlayingPage final : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(NowPlayingPage)

public:
  NowPlayingPage(QWidget* parent);
  ~NowPlayingPage() = default;

public slots:
  void refresh();

protected:
  void showEvent(QShowEvent* event) override;

private:
  QString playingHtml() const;
  QString idleHtml() const;
  void openLink(const QUrl& url);

  PosterWidget* m_poster = nullptr;
  QTextBrowser* m_browser = nullptr;
  int m_animeId = 0;
};

}  // namespace gui
