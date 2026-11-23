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

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QList>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "gui/common/page_widget.hpp"
#include "media/anime_stats.hpp"

namespace gui {

class ActivityChart;

class ProfileWidget final : public PageWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ProfileWidget)

public:
  ProfileWidget(QWidget* parent);
  ~ProfileWidget() = default;

  void refresh();
  void refreshAccount();
  void refreshActivity();
  void refreshLocalDatabase();

protected:
  void changeEvent(QEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;

private:
  void alignForms();
  void arrangeColumns();
  void openLink(const QString& link);

  QWidget* m_container = nullptr;
  QScrollArea* m_scrollArea = nullptr;
  QBoxLayout* m_columns = nullptr;
  QVBoxLayout* m_leftColumn = nullptr;
  QVBoxLayout* m_rightColumn = nullptr;
  QList<QGroupBox*> m_leftGroups;
  QList<QGroupBox*> m_rightGroups;
  int m_columnCount = 0;

  QLabel* m_avatar = nullptr;
  QLabel* m_userName = nullptr;
  QLabel* m_userService = nullptr;
  QPushButton* m_userButton = nullptr;
  QString m_userUrl;

  QGridLayout* m_tiles = nullptr;
  QList<QFrame*> m_tileFrames;
  QLabel* m_weekEpisodes = nullptr;
  QLabel* m_streak = nullptr;
  QLabel* m_readyEpisodes = nullptr;
  QLabel* m_airingSoon = nullptr;

  QLabel* m_airing = nullptr;
  QLabel* m_ready = nullptr;
  ActivityChart* m_activityChart = nullptr;
  QLabel* m_recent = nullptr;
  QTimer* m_activityTimer = nullptr;

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
  QList<QFormLayout*> m_forms;
};

}  // namespace gui
