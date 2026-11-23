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

#include <QDateTime>
#include <QDesktopServices>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>
#include <ranges>
#include <set>

#include "akyuu/accounts.hpp"
#include "gui/media/media_dialog.hpp"
#include "gui/settings/settings_dialog.hpp"
#include "gui/utils/format.hpp"
#include "media/anime_db.hpp"
#include "media/anime_history.hpp"
#include "media/anime_list.hpp"
#include "media/anime_utils.hpp"
#include "sync/service.hpp"
#include "track/library.hpp"

namespace gui {

using namespace Qt::StringLiterals;

namespace {

// Days shown by the activity chart, two weeks to compare one with the other
constexpr int kActivityDays = 14;
// Rows of each list on the page, which point to the full pages for the rest
constexpr int kListLimit = 8;
constexpr int kRecentLimit = 6;
// In characters, below which the page has a single column
constexpr int kTwoColumnWidth = 110;

QLabel* createRichLabel(QWidget* parent) {
  const auto label = new QLabel(parent);
  label->setTextFormat(Qt::RichText);
  label->setWordWrap(true);
  label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  label->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
  return label;
}

QString escaped(const std::string& text) {
  return QString::fromStdString(text).toHtmlEscaped();
}

QString animeLink(const anime::Details& anime) {
  return u"<a href=\"akyuu:info?id=%1\">%2</a>"_s.arg(anime.id).arg(
      escaped(anime::preferredTitle(anime)));
}

// A row of a list, with the title on the left and what is said about it on the right
QString tableRow(const QString& left, const QString& right) {
  return u"<tr><td style=\"padding: 2px 12px 2px 0;\">%1</td>"
         u"<td align=\"right\" style=\"padding: 2px 0; white-space: nowrap;\">%2</td></tr>"_s.arg(
             left, right);
}

QString table(const QStringList& rows) {
  return u"<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">%1</table>"_s.arg(
      rows.join(QString{}));
}

QString muted(const QString& text) {
  return u"<span style=\"color: palette(placeholder-text);\">%1</span>"_s.arg(text);
}

QString note(const QString& text) {
  return u"<p style=\"margin: 0;\">%1</p>"_s.arg(text.toHtmlEscaped());
}

// Rich text sizes such as `x-large` do not grow the text of a label, so the size is given in
// points, from the font of the page, which follows its zoom
QString largeText(const QWidget* widget, const QString& text, const double scale) {
  const auto font = widget->font();
  const auto size = font.pointSizeF() > 0 ? u"%1pt"_s.arg(font.pointSizeF() * scale, 0, 'f', 1)
                                          : u"%1px"_s.arg(qRound(font.pixelSize() * scale));
  return u"<span style=\"font-size: %1; font-weight: 600;\">%2</span>"_s.arg(size, text);
}

QString formatEpisodeRange(const int first, const int last) {
  return first == last ? QCoreApplication::translate("gui::ProfileWidget", "Episode %1").arg(first)
                       : QCoreApplication::translate("gui::ProfileWidget", "Episodes %1-%2")
                             .arg(first)
                             .arg(last);
}

// Local days, which is how the user counts them
QDate localDate(const std::time_t time) {
  return QDateTime::fromSecsSinceEpoch(time).toLocalTime().date();
}

// The episodes of each of the last days, the oldest first
std::vector<int> episodesPerDay(const QDate today) {
  std::vector<int> days(kActivityDays, 0);
  for (const auto& item : anime::history.items()) {
    if (item.episode <= 0) continue;
    const auto age = localDate(item.time).daysTo(today);
    if (age >= 0 && age < kActivityDays) ++days[kActivityDays - 1 - age];
  }
  return days;
}

// Days in a row with an episode, up to today. Today still counts as part of the streak while it
// has nothing yet, since the day is not over.
int streakDays(const QDate today) {
  std::set<QDate> dates;
  for (const auto& item : anime::history.items()) {
    if (item.episode > 0) dates.insert(localDate(item.time));
  }
  auto day = dates.contains(today) ? today : today.addDays(-1);
  int streak = 0;
  while (dates.contains(day)) {
    ++streak;
    day = day.addDays(-1);
  }
  return streak;
}

// The episodes after the last watched one that are in the library folders
std::vector<track::EpisodeRange> unwatchedRanges(const anime::list::Entry& entry) {
  std::vector<track::EpisodeRange> ranges;
  for (auto [first, last] : track::library()->availableEpisodeRanges(entry.anime_id)) {
    first = std::max(first, entry.watched_episodes + 1);
    if (first <= last) ranges.emplace_back(first, last);
  }
  return ranges;
}

int rangeSize(const std::vector<track::EpisodeRange>& ranges) {
  int count = 0;
  for (const auto& [first, last] : ranges) count += last - first + 1;
  return count;
}

}  // namespace

// Bars for the episodes of each of the last two weeks, with the days under them. The count of a day
// is written over its bar, so no axis is needed.
class ActivityChart final : public QWidget {
public:
  explicit ActivityChart(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

  void setDays(const QDate today, std::vector<int> days) {
    m_today = today;
    m_days = std::move(days);
    update();
  }

  QSize sizeHint() const override {
    const auto line = fontMetrics().height();
    return {line * kActivityDays * 2, line * 7};
  }

  QSize minimumSizeHint() const override {
    const auto line = fontMetrics().height();
    return {line * kActivityDays, line * 7};
  }

protected:
  void paintEvent(QPaintEvent*) override {
    if (m_days.empty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto fm = fontMetrics();
    const auto line = fm.height();
    const auto maximum = std::max(*std::ranges::max_element(m_days), 1);
    const auto slot = static_cast<double>(width()) / kActivityDays;
    const auto barWidth = std::clamp(slot * 0.6, 4.0, line * 1.5);
    const auto chartTop = line + 2.0;
    const auto chartBottom = height() - line - 6.0;
    const auto chartHeight = chartBottom - chartTop;
    const auto radius = std::min(4.0, barWidth / 2);

    for (int i = 0; i < kActivityDays; ++i) {
      const auto count = m_days[i];
      const auto date = m_today.addDays(i - kActivityDays + 1);
      const auto center = slot * i + slot / 2;
      const auto isToday = i == kActivityDays - 1;

      // An empty day still gets a mark, so the days line up with their names
      const auto barHeight = count ? std::max(chartHeight * count / maximum, radius * 2) : 3.0;
      const QRectF bar{center - barWidth / 2, chartBottom - barHeight, barWidth, barHeight};
      QPainterPath path;
      path.addRoundedRect(bar, std::min(radius, barHeight / 2), std::min(radius, barHeight / 2));
      auto color = palette().color(count ? QPalette::Highlight : QPalette::Mid);
      if (count && !isToday) color.setAlphaF(0.75);
      painter.fillPath(path, color);

      painter.setPen(palette().color(QPalette::WindowText));
      if (count) {
        const QRectF label{center - slot / 2, bar.top() - line - 2, slot, static_cast<qreal>(line)};
        painter.drawText(label, Qt::AlignCenter, QString::number(count));
      }

      auto font = painter.font();
      font.setBold(isToday);
      painter.setFont(font);
      painter.setPen(palette().color(isToday ? QPalette::WindowText : QPalette::PlaceholderText));
      const QRectF dayLabel{center - slot / 2, chartBottom + 4, slot, static_cast<qreal>(line)};
      painter.drawText(dayLabel, Qt::AlignCenter,
                       QLocale::system().dayName(date.dayOfWeek(), QLocale::ShortFormat));
      font.setBold(false);
      painter.setFont(font);
    }
  }

private:
  QDate m_today;
  std::vector<int> m_days;
};

ProfileWidget::ProfileWidget(QWidget* parent) : PageWidget(parent) {
  const auto container = new QWidget(this);
  const auto containerLayout = new QVBoxLayout(container);
  containerLayout->setContentsMargins(16, 12, 16, 16);
  containerLayout->setSpacing(12);

  // Account
  {
    const auto header = new QWidget(container);
    const auto layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    m_avatar = new QLabel(header);
    m_avatar->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_avatar);

    const auto names = new QVBoxLayout();
    names->setSpacing(0);
    m_userName = new QLabel(header);
    m_userName->setTextFormat(Qt::RichText);
    m_userService = new QLabel(header);
    m_userService->setObjectName("profileCaption");
    names->addWidget(m_userName);
    names->addWidget(m_userService);
    // Centered next to the avatar without spacers, which would take the height left on the page
    layout->addLayout(names, 1);
    layout->setAlignment(names, Qt::AlignVCenter);

    m_userButton = new QPushButton(header);
    connect(m_userButton, &QPushButton::clicked, this, [this]() {
      if (m_userUrl.isEmpty()) {
        SettingsDialog::show(this, SettingsPageId::Accounts);
      } else {
        QDesktopServices::openUrl(QUrl{m_userUrl});
      }
    });
    layout->addWidget(m_userButton, 0, Qt::AlignVCenter);

    containerLayout->addWidget(header);
  }

  // At a glance, in a row, or two rows in a narrow window
  {
    m_tiles = new QGridLayout();
    m_tiles->setSpacing(12);
    const auto addTile = [this, container](const QString& caption, const QString& tooltip) {
      const auto tile = new QFrame(container);
      tile->setObjectName("profileTile");
      tile->setToolTip(tooltip);
      const auto layout = new QVBoxLayout(tile);
      layout->setContentsMargins(12, 8, 12, 8);
      layout->setSpacing(0);
      const auto value = new QLabel("-", tile);
      value->setTextFormat(Qt::RichText);
      const auto text = new QLabel(caption, tile);
      text->setObjectName("profileCaption");
      layout->addWidget(value);
      layout->addWidget(text);
      m_tileFrames.append(tile);
      return value;
    };
    m_weekEpisodes = addTile(tr("Episodes this week"), tr("Episodes watched in the last 7 days"));
    m_streak = addTile(tr("Day streak"), tr("Days in a row with at least one episode watched"));
    m_readyEpisodes =
        addTile(tr("Ready to watch"), tr("Unwatched episodes of your list in the library folders"));
    m_airingSoon = addTile(tr("Airing this week"),
                           tr("Anime you are watching with an episode out in the next 7 days"));
    containerLayout->addLayout(m_tiles);
  }

  // Two columns side by side in a wide window, what comes next on the left and what has been
  // watched on the right. A narrow window stacks them, see `arrangeColumns`.
  {
    m_columns = new QBoxLayout(QBoxLayout::LeftToRight);
    m_columns->setSpacing(12);
    m_leftColumn = new QVBoxLayout();
    m_rightColumn = new QVBoxLayout();
    m_leftColumn->setSpacing(12);
    m_rightColumn->setSpacing(12);
    m_columns->addLayout(m_leftColumn, 1);
    m_columns->addLayout(m_rightColumn, 1);
    containerLayout->addLayout(m_columns);
  }

  const auto addListGroup = [this, container](const QString& title, QLabel*& label) {
    const auto group = new QGroupBox(title, container);
    const auto layout = new QVBoxLayout(group);
    label = createRichLabel(group);
    connect(label, &QLabel::linkActivated, this, &ProfileWidget::openLink);
    layout->addWidget(label);
    m_leftGroups.append(group);
    return group;
  };

  // Airing next
  addListGroup(tr("Airing next"), m_airing);

  // Ready to watch
  addListGroup(tr("Ready to watch"), m_ready);

  // Recent activity, on the right in a wide window
  {
    const auto group = addListGroup(tr("Recent activity"), m_recent);
    m_activityChart = new ActivityChart(group);
    static_cast<QVBoxLayout*>(group->layout())->insertWidget(0, m_activityChart);
    m_leftGroups.removeLast();
    m_rightGroups.append(group);
  }

  // The lists are rebuilt rather than updated, so their countdowns move once a minute
  m_activityTimer = new QTimer(this);
  m_activityTimer->setInterval(std::chrono::minutes{1});
  connect(m_activityTimer, &QTimer::timeout, this, &ProfileWidget::refreshActivity);

  const auto refreshIfVisible = [this]() {
    if (isVisible()) refreshActivity();
  };
  connect(&anime::history, &anime::History::changed, this, refreshIfVisible);
  connect(&anime::db, &anime::Database::entryUpdated, this, refreshIfVisible);
  connect(track::library(), &track::Library::availabilityChanged, this, refreshIfVisible);
  connect(track::library(), &track::Library::scanCompleted, this, refreshIfVisible);

  containerLayout->addStretch();
  arrangeColumns();

  // The page scrolls rather than squeezing the groups into a small window
  const auto scrollArea = new QScrollArea(this);
  m_container = container;
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
}

// The name of the account of the service in use, and where its own profile is
void ProfileWidget::refreshAccount() {
  const auto service = services::currentServiceId();
  QString user;
  switch (service) {
    case services::ServiceId::AniList:
      user = QString::fromStdString(akyuu::accounts.anilistUsername());
      m_userUrl = u"https://anilist.co/user/%1"_s.arg(user);
      break;
    case services::ServiceId::Kitsu:
      user = QString::fromStdString(akyuu::accounts.kitsuUsername());
      m_userUrl = u"https://kitsu.app/users/%1"_s.arg(user);
      break;
    case services::ServiceId::MyAnimeList:
      user = QString::fromStdString(akyuu::accounts.myanimelistUsername());
      m_userUrl = u"https://myanimelist.net/profile/%1"_s.arg(user);
      break;
    default:
      break;
  }
  if (user.isEmpty()) m_userUrl.clear();

  const auto serviceName = services::serviceName(service);
  const auto name = user.isEmpty() ? tr("Not signed in") : user;
  m_userName->setText(largeText(this, name.toHtmlEscaped(), 1.8));
  m_userService->setText(user.isEmpty() ? tr("Sign in to sync your list")
                                        : tr("Synced with %1").arg(serviceName));
  m_userButton->setText(user.isEmpty() ? tr("Set up an account")
                                       : tr("View on %1").arg(serviceName));

  // The first letter of the name in a circle, sized by the font so that it follows the zoom
  const auto size = fontMetrics().height() * 3;
  const auto ratio = devicePixelRatioF();
  QPixmap pixmap(QSize{size, size} * ratio);
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);
  {
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Highlight));
    painter.drawEllipse(QRectF{0, 0, static_cast<qreal>(size), static_cast<qreal>(size)});
    auto font = this->font();
    font.setPixelSize(size / 2);
    font.setWeight(QFont::Weight::DemiBold);
    painter.setFont(font);
    painter.setPen(palette().color(QPalette::HighlightedText));
    painter.drawText(QRect{0, 0, size, size}, Qt::AlignCenter,
                     user.isEmpty() ? u"?"_s : user.left(1).toUpper());
  }
  m_avatar->setPixmap(pixmap);
  m_avatar->setFixedSize(size, size);
}

void ProfileWidget::refreshActivity() {
  const auto now = QDateTime::currentSecsSinceEpoch();
  const auto today = QDate::currentDate();
  const auto& entries = anime::db.entries();

  const auto tileValue = [this](const int value) {
    return largeText(this, QString::number(value), 2.0);
  };

  // Recent activity
  const auto days = episodesPerDay(today);
  int weekEpisodes = 0;
  for (int i = kActivityDays - 7; i < kActivityDays; ++i) weekEpisodes += days[i];
  m_activityChart->setDays(today, days);
  m_weekEpisodes->setText(tileValue(weekEpisodes));
  m_streak->setText(tileValue(streakDays(today)));

  {
    QStringList rows;
    const auto& items = anime::history.items();
    for (auto it = items.rbegin(); it != items.rend() && rows.size() < kRecentLimit; ++it) {
      if (it->episode <= 0) continue;
      const auto anime = anime::db.item(it->anime_id);
      if (!anime) continue;
      rows.append(tableRow(animeLink(*anime) + u" "_s + muted(u"#%1"_s.arg(it->episode)),
                           muted(formatAsRelativeTime(it->time))));
    }
    m_recent->setText(rows.isEmpty() ? note(tr("Episodes you watch show up here.")) : table(rows));
  }

  // Airing next, from what the service said about the anime being watched
  {
    std::vector<const anime::Details*> airing;
    for (const auto& entry : entries) {
      if (entry.status != anime::list::Status::Watching) continue;
      const auto anime = anime::db.item(entry.anime_id);
      if (anime && anime->next_episode_time > now) airing.push_back(anime);
    }
    std::ranges::sort(airing, {}, &anime::Details::next_episode_time);

    const auto week = now + 7 * 24 * 60 * 60;
    m_airingSoon->setText(tileValue(
        static_cast<int>(std::ranges::count_if(airing, [week](const anime::Details* anime) {
          return anime->next_episode_time <= week;
        }))));

    QStringList rows;
    for (const auto anime : airing | std::views::take(kListLimit)) {
      const auto time = QDateTime::fromSecsSinceEpoch(anime->next_episode_time).toLocalTime();
      const auto when = QLocale::system().toString(time, u"ddd HH:mm"_s);
      rows.append(tableRow(animeLink(*anime),
                           formatAsRelativeTime(anime->next_episode_time) + u"  "_s + muted(when)));
    }
    m_airing->setText(rows.isEmpty()
                          ? note(tr("Nothing you are watching has an episode on the way."))
                          : table(rows));
  }

  // Ready to watch, from the library folders
  {
    struct Ready {
      const anime::Details* anime;
      const anime::list::Entry* entry;
      std::vector<track::EpisodeRange> ranges;
    };
    std::vector<Ready> ready;
    int readyEpisodes = 0;
    for (const auto& entry : entries) {
      using enum anime::list::Status;
      if (entry.status == Completed || entry.status == Dropped) continue;
      const auto anime = anime::db.item(entry.anime_id);
      if (!anime) continue;
      auto ranges = unwatchedRanges(entry);
      if (ranges.empty()) continue;
      readyEpisodes += rangeSize(ranges);
      ready.push_back({anime, &entry, std::move(ranges)});
    }
    // What is being watched first, then what was touched last
    std::ranges::sort(ready, [](const Ready& a, const Ready& b) {
      const auto watchingA = a.entry->status == anime::list::Status::Watching;
      const auto watchingB = b.entry->status == anime::list::Status::Watching;
      if (watchingA != watchingB) return watchingA;
      return a.entry->last_updated > b.entry->last_updated;
    });

    const auto scanned = track::library()->hasScanned();
    m_readyEpisodes->setText(scanned ? tileValue(readyEpisodes) : tileValue(0));

    QStringList rows;
    for (const auto& item : ready | std::views::take(kListLimit)) {
      QStringList ranges;
      for (const auto& [first, last] : item.ranges) {
        ranges.append(formatEpisodeRange(first, last));
      }
      rows.append(tableRow(animeLink(*item.anime), muted(ranges.join(u", "_s))));
    }
    if (std::ssize(ready) > kListLimit) {
      rows.append(tableRow(
          muted(tr("and %n more", nullptr, static_cast<int>(std::ssize(ready) - kListLimit))), {}));
    }
    if (!scanned) {
      m_ready->setText(note(tr("The library folders have not been scanned yet.")));
    } else {
      m_ready->setText(rows.isEmpty() ? note(tr("No unwatched episodes in the library folders."))
                                      : table(rows));
    }
  }
}

void ProfileWidget::openLink(const QString& link) {
  const QUrl url{link};
  if (url.scheme() != u"akyuu"_s || url.path() != u"info"_s) return;
  const auto id = QUrlQuery(url).queryItemValue(u"id"_s).toInt();
  if (const auto anime = anime::db.item(id)) {
    MediaDialog::show(this, MediaDialogPage::Details, *anime);
  }
}

// Side by side when both columns have room for their lists, one under the other otherwise
void ProfileWidget::arrangeColumns() {
  const auto available = m_scrollArea ? m_scrollArea->viewport()->width() : width();
  const auto columnCount = available >= fontMetrics().averageCharWidth() * kTwoColumnWidth ? 2 : 1;
  if (columnCount == m_columnCount) return;
  m_columnCount = columnCount;

  const auto tilesPerRow = columnCount == 2 ? 4 : 2;
  for (int i = 0; i < m_tileFrames.size(); ++i) {
    m_tiles->addWidget(m_tileFrames[i], i / tilesPerRow, i % tilesPerRow);
  }
  for (int i = 0; i < 4; ++i) m_tiles->setColumnStretch(i, i < tilesPerRow ? 1 : 0);

  for (const auto layout : {m_leftColumn, m_rightColumn}) {
    while (layout->count()) delete layout->takeAt(0);
  }
  for (const auto group : m_leftGroups) m_leftColumn->addWidget(group);
  const auto right = columnCount == 2 ? m_rightColumn : m_leftColumn;
  for (const auto group : m_rightGroups) right->addWidget(group);
  m_leftColumn->addStretch();
  m_rightColumn->addStretch();
  m_columns->setStretch(1, columnCount == 2 ? 1 : 0);
}

void ProfileWidget::hideEvent(QHideEvent* event) {
  m_activityTimer->stop();
  PageWidget::hideEvent(event);
}

void ProfileWidget::showEvent(QShowEvent* event) {
  refreshAccount();
  refreshActivity();
  m_activityTimer->start();
  PageWidget::showEvent(event);
  arrangeColumns();
}

void ProfileWidget::changeEvent(QEvent* event) {
  PageWidget::changeEvent(event);
  // The labels take the new font of a zoomed page only afterwards, and the large text and the
  // avatar are sized from it
  if (event->type() == QEvent::FontChange) {
    QMetaObject::invokeMethod(
        this,
        [this]() {
          m_columnCount = 0;
          arrangeColumns();
          if (!isVisible()) return;
          refreshAccount();
          refreshActivity();
        },
        Qt::QueuedConnection);
  }
}

void ProfileWidget::resizeEvent(QResizeEvent* event) {
  PageWidget::resizeEvent(event);
  arrangeColumns();
}

}  // namespace gui
