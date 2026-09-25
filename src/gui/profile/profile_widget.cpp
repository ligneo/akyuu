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

#include "profile_widget.hpp"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>

#include "akyuu/application.hpp"
#include "akyuu/session.hpp"
#include "base/chrono.hpp"
#include "base/string.hpp"
#include "gui/utils/format.hpp"
#include "media/anime_list.hpp"

namespace gui {

ProfileWidget::ProfileWidget(QWidget* parent) : PageWidget(parent) {
  const auto container = new QWidget(this);
  const auto containerLayout = new QVBoxLayout(container);
  containerLayout->setContentsMargins(16, 8, 16, 16);

  // Anime list
  {
    const auto group = new QGroupBox(tr("Anime list"), container);
    const auto form = new QFormLayout(group);

    const auto add_row = [this, form](const QString& text) {
      const auto label = new QLabel("-", form->parentWidget());
      form->addRow(text, label);
      return label;
    };

    m_animeCount = add_row(tr("Anime count:"));
    m_episodeCount = add_row(tr("Episode count:"));
    m_timeSpentWatching = add_row(tr("Time spent watching:"));
    m_timeToComplete = add_row(tr("Time to complete:"));
    m_scoreMean = add_row(tr("Mean score:"));
    m_scoreDeviation = add_row(tr("Score deviation:"));

    containerLayout->addWidget(group);
  }

  // Score distribution
  {
    const auto group = new QGroupBox(tr("Score distribution"), container);
    const auto grid = new QGridLayout(group);
    grid->setColumnStretch(1, 1);

    for (size_t i = anime::kScoreBucketCount; i > 1; --i) {
      const auto score = static_cast<int>(i - 1);
      const auto row = grid->rowCount();

      const auto label = new QLabel(QString::number(score), group);
      label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

      const auto bar = new QProgressBar(group);
      bar->setTextVisible(false);
      bar->setFixedHeight(16);

      const auto count = new QLabel("0", group);

      grid->addWidget(label, row, 0);
      grid->addWidget(bar, row, 1);
      grid->addWidget(count, row, 2);

      m_scoreBars.append(bar);
      m_scoreCounts.append(count);
    }

    containerLayout->addWidget(group);
  }

  // Akyuu
  // v1's own statistics block also counts connections (`stats.connections_*`), which nothing in
  // v2 keeps track of yet.
  {
    const auto group = new QGroupBox(tr("Akyuu"), container);
    const auto form = new QFormLayout(group);

    m_uptime = new QLabel("-", group);
    m_tigersHarmed = new QLabel("-", group);

    form->addRow(tr("Uptime:"), m_uptime);
    form->addRow(tr("Tigers harmed:"), m_tigersHarmed);

    containerLayout->addWidget(group);
  }

  // v1 counts the uptime in its one-second timer and refreshes the whole statistics dialog while
  // it is visible (`timer.cpp:163`), so everything on it stays current, not just the clock.
  // Recomputing costs ~20 microseconds on a 479 entry list, which is nothing once a second.
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setInterval(std::chrono::seconds{1});
  connect(m_refreshTimer, &QTimer::timeout, this, &ProfileWidget::refresh);

  containerLayout->addStretch();
  layout()->addWidget(container);

  m_toolbar->hide();
}

void ProfileWidget::refresh() {
  const auto stats = anime::computeStatistics();

  m_animeCount->setText(QString::number(stats.anime_count));
  m_episodeCount->setText(QString::number(stats.episode_count));
  m_timeSpentWatching->setText(formatTimeSpan(base::Duration{stats.time_spent_watching}));
  m_timeToComplete->setText(formatTimeSpan(base::Duration{stats.time_to_complete}));
  // Scores are stored in the 0-100 range, but users are used to seeing them out of ten.
  const auto format_score = [](const float value) { return QString::number(value / 10.0, 'f', 2); };
  m_scoreMean->setText(format_score(stats.score_mean));
  m_scoreDeviation->setText(format_score(stats.score_deviation));

  const auto maximum = *std::ranges::max_element(stats.score_count);

  for (qsizetype i = 0; i < m_scoreBars.size(); ++i) {
    // Bars are listed from the highest score to the lowest
    const auto bucket = stats.score_count.size() - 1 - static_cast<size_t>(i);
    const auto count = stats.score_count[bucket];

    m_scoreBars[i]->setRange(0, std::max(maximum, 1));
    m_scoreBars[i]->setValue(count);
    m_scoreCounts[i]->setText(QString::number(count));
  }

  m_uptime->setText(formatDuration(akyuu::app()->uptime()));
  m_tigersHarmed->setText(QString::number(akyuu::session.tigersHarmed()));
}

void ProfileWidget::hideEvent(QHideEvent* event) {
  m_refreshTimer->stop();
  PageWidget::hideEvent(event);
}

void ProfileWidget::showEvent(QShowEvent* event) {
  refresh();
  m_refreshTimer->start();
  PageWidget::showEvent(event);
}

}  // namespace gui
