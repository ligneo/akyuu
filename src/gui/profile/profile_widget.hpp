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

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "gui/common/page_widget.hpp"

namespace gui {

class ActivityChart;
class Timeline;
class ProfileHeader;
class WeekSchedule;

class ProfileWidget final : public PageWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ProfileWidget)

public:
  ProfileWidget(QWidget* parent);
  ~ProfileWidget() = default;

  void refreshAccount();
  void refreshActivity();

protected:
  void changeEvent(QEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;

private:
  void arrangeColumns();
  void balanceColumns();
  void openLink(const QString& link);

  QWidget* m_container = nullptr;
  QScrollArea* m_scrollArea = nullptr;
  QBoxLayout* m_columns = nullptr;
  QVBoxLayout* m_leftColumn = nullptr;
  QVBoxLayout* m_rightColumn = nullptr;
  QList<QGroupBox*> m_sections;
  int m_columnCount = 0;
  bool m_buttonUnder = false;
  bool m_buttonPlaced = false;

  ProfileHeader* m_header = nullptr;
  QHBoxLayout* m_headerLayout = nullptr;
  QVBoxLayout* m_names = nullptr;
  QLabel* m_avatar = nullptr;
  QLabel* m_userName = nullptr;
  QLabel* m_userService = nullptr;
  QPushButton* m_userButton = nullptr;
  QString m_userUrl;
  QString m_accountState;

  QGridLayout* m_tiles = nullptr;
  QList<QFrame*> m_tileFrames;
  QLabel* m_weekEpisodes = nullptr;
  QLabel* m_streak = nullptr;
  QLabel* m_weekTime = nullptr;
  QLabel* m_completedYear = nullptr;

  QGroupBox* m_airingGroup = nullptr;
  QLabel* m_airing = nullptr;
  WeekSchedule* m_airingSchedule = nullptr;
  QLabel* m_airingLater = nullptr;
  QGroupBox* m_upcomingGroup = nullptr;
  QLabel* m_upcoming = nullptr;
  Timeline* m_upcomingTimeline = nullptr;
  QLabel* m_ready = nullptr;
  Timeline* m_readyTimeline = nullptr;
  ActivityChart* m_activityChart = nullptr;
  QLabel* m_recent = nullptr;
  Timeline* m_recentTimeline = nullptr;
  QTimer* m_activityTimer = nullptr;
};

}  // namespace gui
