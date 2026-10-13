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

#include <QDir>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLocale>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <chrono>

#include "akyuu/application.hpp"
#include "akyuu/path.hpp"
#include "akyuu/session.hpp"
#include "akyuu/settings.hpp"
#include "base/chrono.hpp"
#include "base/string.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/image_provider.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"

namespace gui {

using namespace Qt::StringLiterals;

namespace {

// Zoom levels in percent, like the zoom of a document
constexpr int kZoomMin = 70;
constexpr int kZoomMax = 200;
constexpr int kZoomStep = 10;

// Shrinking to fit stops here; smaller text is hard to read, so the page scrolls instead
constexpr double kFitMin = 0.8;

constexpr int kBarHeight = 16;

}  // namespace

ProfileWidget::ProfileWidget(QWidget* parent) : PageWidget(parent) {
  const auto container = new QWidget(this);
  m_container = container;
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
      bar->setFixedHeight(kBarHeight);

      const auto count = new QLabel("0", group);

      grid->addWidget(label, row, 0);
      grid->addWidget(bar, row, 1);
      grid->addWidget(count, row, 2);

      m_scoreBars.append(bar);
      m_scoreCounts.append(count);
    }

    containerLayout->addWidget(group);
  }

  // Local database
  // v1's `dlg_stats.cpp:220`: what the local database and the caches next to it hold.
  {
    const auto group = new QGroupBox(tr("Local database"), container);
    const auto form = new QFormLayout(group);

    m_animeItems = new QLabel("-", group);
    m_imageFiles = new QLabel("-", group);
    m_torrentFiles = new QLabel("-", group);

    form->addRow(tr("Anime items:"), m_animeItems);
    form->addRow(tr("Image files:"), m_imageFiles);
    form->addRow(tr("Torrent files:"), m_torrentFiles);

    containerLayout->addWidget(group);
  }

  // Akyuu
  // v1's own statistics block also counts connections (`stats.connections_*`), which nothing in
  // v2 keeps track of yet.
  {
    const auto group = new QGroupBox(tr("Akyuu"), container);
    const auto form = new QFormLayout(group);

    m_uptime = new QLabel("-", group);
    m_owlsWoken = new QLabel("-", group);

    form->addRow(tr("Uptime:"), m_uptime);
    form->addRow(tr("Owls woken:"), m_owlsWoken);

    containerLayout->addWidget(group);
  }

  // v1 counts the uptime in its one-second timer and refreshes the whole statistics dialog while
  // it is visible (`timer.cpp:163`), so everything on it stays current, not just the clock.
  // Recomputing costs ~20 microseconds on a 479 entry list, which is nothing once a second.
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setInterval(std::chrono::seconds{1});
  connect(m_refreshTimer, &QTimer::timeout, this, &ProfileWidget::refresh);

  containerLayout->addStretch();

  // The page is taller than a small window, so it scrolls instead of squeezing the groups
  const auto scrollArea = new QScrollArea(this);
  m_scrollArea = scrollArea;
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setWidgetResizable(true);
  // Themes give the scroll areas of pages the background of a list, which this page is not
  scrollArea->setStyleSheet(u"QScrollArea { background: transparent; }"_s);
  scrollArea->setWidget(container);
  // `setWidget` fills the background of the container, which the window already paints
  container->setAutoFillBackground(false);
  layout()->addWidget(scrollArea);

  m_toolbar->hide();

  // Ctrl and the mouse wheel zoom, see `eventFilter`
  m_scrollArea->viewport()->installEventFilter(this);

  m_baseFont = container->font();
  m_zoom = std::clamp(akyuu::settings.statisticsZoom(), kZoomMin, kZoomMax);
  updateScale();
}

void ProfileWidget::zoomIn() {
  setZoom(m_zoom + kZoomStep);
}

void ProfileWidget::zoomOut() {
  setZoom(m_zoom - kZoomStep);
}

void ProfileWidget::resetZoom() {
  setZoom(100);
}

void ProfileWidget::setZoom(const int zoom) {
  const auto clamped = std::clamp(zoom, kZoomMin, kZoomMax);
  if (clamped == m_zoom) return;
  m_zoom = clamped;
  akyuu::settings.setStatisticsZoom(m_zoom);
  updateScale();
}

// The zoom sets the size, and shrinking to fit, if it is turned on, takes it down from there
// until the page fits the window.
void ProfileWidget::updateScale() {
  const auto zoom = m_zoom / 100.0;

  if (!akyuu::settings.statisticsFitToWindow()) {
    applyScale(zoom);
    return;
  }

  const auto available = m_scrollArea->viewport()->height();
  for (auto fit = 1.0; fit >= kFitMin; fit -= 0.05) {
    applyScale(zoom * fit);
    if (m_container->sizeHint().height() <= available) return;
  }
  applyScale(zoom * kFitMin);
}

void ProfileWidget::applyScale(const double scale) {
  auto font = m_baseFont;
  if (m_baseFont.pointSizeF() > 0) {
    font.setPointSizeF(m_baseFont.pointSizeF() * scale);
  } else {
    font.setPixelSize(std::max(1, qRound(m_baseFont.pixelSize() * scale)));
  }

  // The application style sheet keeps fonts from reaching child widgets, so each gets its own
  m_container->setFont(font);
  for (const auto child : m_container->findChildren<QWidget*>()) {
    child->setFont(font);
  }
  for (const auto bar : m_scoreBars) {
    bar->setFixedHeight(qRound(kBarHeight * scale));
  }
  m_container->layout()->setContentsMargins(qRound(16 * scale), qRound(8 * scale),
                                            qRound(16 * scale), qRound(16 * scale));
  m_container->layout()->invalidate();
}

// The settings dialog is modal, so the window becomes active again once it closes. Shrinking to fit
// may have been turned on or off there.
void ProfileWidget::changeEvent(QEvent* event) {
  if (event->type() == QEvent::ActivationChange && isVisible()) updateScale();
  PageWidget::changeEvent(event);
}

bool ProfileWidget::eventFilter(QObject* watched, QEvent* event) {
  // The scroll area would turn Ctrl and the wheel into scrolling by pages
  if (watched == m_scrollArea->viewport() && event->type() == QEvent::Wheel) {
    const auto wheelEvent = static_cast<QWheelEvent*>(event);
    if (wheelEvent->modifiers() & Qt::ControlModifier) {
      const auto delta = wheelEvent->angleDelta().y();
      if (delta > 0) zoomIn();
      if (delta < 0) zoomOut();
      return true;
    }
  }
  return PageWidget::eventFilter(watched, event);
}

void ProfileWidget::resizeEvent(QResizeEvent* event) {
  PageWidget::resizeEvent(event);
  if (akyuu::settings.statisticsFitToWindow()) updateScale();
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
  m_owlsWoken->setText(QString::number(akyuu::session.owlsWoken()));
}

// Walks two folders, so it runs when the page is shown rather than with the one-second refresh.
void ProfileWidget::refreshLocalDatabase() {
  const auto describeFolder = [](const QString& path) {
    const auto files = QDir{path}.entryInfoList(QDir::Files);
    qint64 size = 0;
    for (const auto& file : files) {
      size += file.size();
    }
    return u"%1 (%2)"_s.arg(files.size()).arg(QLocale::system().formattedDataSize(size, 1));
  };

  m_animeItems->setText(QString::number(anime::db.items().size()));
  m_imageFiles->setText(describeFolder(imageProvider.cachePath()));
  m_torrentFiles->setText(
      describeFolder(u"%1/torrents"_s.arg(QString::fromStdString(akyuu::get_data_path()))));
}

void ProfileWidget::hideEvent(QHideEvent* event) {
  m_refreshTimer->stop();
  PageWidget::hideEvent(event);
}

void ProfileWidget::showEvent(QShowEvent* event) {
  // Shrinking to fit may have been turned on or off in the settings
  updateScale();
  refresh();
  refreshLocalDatabase();
  m_refreshTimer->start();
  PageWidget::showEvent(event);
}

}  // namespace gui
