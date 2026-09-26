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

#include <QLabel>
#include <QList>
#include <QProgressBar>

#include "gui/common/page_widget.hpp"
#include "media/anime_stats.hpp"

namespace gui {

class ProfileWidget final : public PageWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ProfileWidget)

public:
  ProfileWidget(QWidget* parent);
  ~ProfileWidget() = default;

  void refresh();
  void refreshLocalDatabase();

protected:
  void hideEvent(QHideEvent* event) override;
  void showEvent(QShowEvent* event) override;

private:
  QLabel* m_animeCount = nullptr;
  QLabel* m_episodeCount = nullptr;
  QLabel* m_timeSpentWatching = nullptr;
  QLabel* m_timeToComplete = nullptr;
  QLabel* m_scoreMean = nullptr;
  QLabel* m_scoreDeviation = nullptr;
  QTimer* m_refreshTimer = nullptr;
  QLabel* m_animeItems = nullptr;
  QLabel* m_imageFiles = nullptr;
  QLabel* m_torrentFiles = nullptr;
  QLabel* m_uptime = nullptr;
  QLabel* m_owlsWoken = nullptr;

  QList<QProgressBar*> m_scoreBars;
  QList<QLabel*> m_scoreCounts;
};

}  // namespace gui
