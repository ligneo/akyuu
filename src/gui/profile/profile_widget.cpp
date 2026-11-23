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
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>
#include <ranges>
#include <set>

#include "akyuu/accounts.hpp"
#include "akyuu/settings.hpp"
#include "gui/main/main_window.hpp"
#include "gui/main/navigation_controller.hpp"
#include "gui/media/media_dialog.hpp"
#include "gui/settings/settings_dialog.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/image_provider.hpp"
#include "media/anime_db.hpp"
#include "media/anime_history.hpp"
#include "media/anime_list.hpp"
#include "media/anime_utils.hpp"
#include "sync/anilist/anilist.hpp"
#include "sync/kitsu/kitsu.hpp"
#include "sync/myanimelist/myanimelist.hpp"
#include "sync/service.hpp"
#include "track/library.hpp"

namespace gui {

using namespace Qt::StringLiterals;

namespace {

// Days shown by the activity chart, two weeks to compare one with the other
constexpr int kActivityDays = 14;
// Rows of a section, which keeps one from pushing the others down
constexpr int kTimelineLimit = 8;
// Episodes of what was recently watched, the History page having all of them
constexpr int kRecentLimit = 100;
// In characters, below which the page has a single column
constexpr int kTwoColumnWidth = 90;

QLabel* createRichLabel(QWidget* parent) {
  const auto label = new QLabel(parent);
  label->setTextFormat(Qt::RichText);
  label->setWordWrap(true);
  label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  label->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
  return label;
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
    return {line * kActivityDays * 2, line * 9 / 2};
  }

  QSize minimumSizeHint() const override {
    const auto line = fontMetrics().height();
    return {line * kActivityDays, line * 9 / 2};
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
      // The first letter alone when the short names would run into each other
      auto day = QLocale::system().dayName(date.dayOfWeek(), QLocale::ShortFormat);
      if (painter.fontMetrics().horizontalAdvance(day) > slot - 4) day = day.left(1);
      painter.drawText(dayLabel, Qt::AlignCenter, day);
      font.setBold(false);
      painter.setFont(font);
    }
  }

private:
  QDate m_today;
  std::vector<int> m_days;
};

// Rows of anime with a small poster, the title and a note under it, and a value on the right. The
// rows can be grouped, by day or by date, on a line down the left that reads as a timeline. A long
// one shows its first rows and opens to the rest, and only the rows on the screen are drawn, so
// that only their posters are loaded however many there are.
struct TimelineRow {
  int id = 0;
  QString note;
  QString value;
};

struct TimelineGroup {
  QString label;
  std::vector<TimelineRow> rows;
};

class Timeline final : public QWidget {
public:
  Timeline(QWidget* parent, const int limit, std::function<void(int)> clicked)
      : QWidget(parent), m_limit(limit), m_clicked(std::move(clicked)) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&imageProvider, &ImageProvider::posterChanged, this, [this](const int id) {
      for (const auto& line : m_lines) {
        if (line.row && line.row->id == id) return update(lineRect(line));
      }
    });
  }

  void setGroups(std::vector<TimelineGroup> groups) {
    m_groups = std::move(groups);
    layoutLines();
  }

  QSize sizeHint() const override {
    const auto height = m_lines.empty() ? 0 : m_lines.back().y + m_lines.back().height;
    return {fontMetrics().averageCharWidth() * 30, height};
  }

  QSize minimumSizeHint() const override {
    return {fontMetrics().averageCharWidth() * 20, sizeHint().height()};
  }

protected:
  void changeEvent(QEvent* event) override {
    QWidget::changeEvent(event);
    // The rows are sized by the font
    if (event->type() == QEvent::FontChange) layoutLines();
  }

  void paintEvent(QPaintEvent* event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const auto fm = fontMetrics();
    const auto rail = hasRail() ? railWidth() : 0;
    const auto railX = rail / 2.0;
    const auto muted = palette().color(QPalette::PlaceholderText);

    // The line between the dots of the groups, which is cheap enough to draw whole
    if (rail) {
      auto color = muted;
      color.setAlphaF(0.4);
      painter.setPen(QPen{color, 2});
      std::optional<qreal> previous;
      for (const auto& line : m_lines) {
        if (line.kind != Line::Kind::Label) continue;
        const auto center = line.y + line.height / 2.0;
        if (previous) painter.drawLine(QPointF{railX, *previous}, QPointF{railX, center});
        previous = center;
      }
    }

    for (int i = 0; i < std::ssize(m_lines); ++i) {
      const auto& line = m_lines[i];
      const auto rect = lineRect(line);
      // Only what is on the screen
      if (!rect.intersects(event->rect())) continue;

      switch (line.kind) {
        case Line::Kind::Label: {
          if (rail) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(palette().color(QPalette::Highlight));
            painter.drawEllipse(QPointF{railX, line.y + line.height / 2.0}, 4, 4);
          }
          auto font = this->font();
          font.setWeight(QFont::Weight::DemiBold);
          painter.setFont(font);
          painter.setPen(palette().color(QPalette::WindowText));
          painter.drawText(rect.adjusted(rail, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                           line.text);
          painter.setFont(this->font());
          break;
        }

        case Line::Kind::More: {
          painter.setPen(palette().color(i == m_hovered ? QPalette::Highlight : QPalette::Link));
          painter.drawText(rect.adjusted(rail, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                           line.text);
          break;
        }

        case Line::Kind::Row: {
          const auto row = rect.adjusted(rail, 0, 0, 0);
          if (i == m_hovered) {
            auto hover = palette().color(QPalette::Highlight);
            hover.setAlphaF(0.15);
            painter.setPen(Qt::NoPen);
            painter.setBrush(hover);
            painter.drawRoundedRect(row.adjusted(-4, 1, 4, -1), 6, 6);
          }

          // The poster, small, as in the lists of v1
          const auto thumbHeight = row.height() - 8;
          const QRectF thumb{static_cast<qreal>(row.left()), row.top() + 4.0, thumbHeight * 2 / 3.0,
                             static_cast<qreal>(thumbHeight)};
          QPainterPath path;
          path.addRoundedRect(thumb, 4, 4);
          painter.save();
          painter.setClipPath(path);
          if (const auto pixmap = imageProvider.loadPoster(line.row->id); !pixmap.isNull()) {
            const auto scaled =
                pixmap.size().scaled(thumb.size().toSize(), Qt::KeepAspectRatioByExpanding);
            painter.drawPixmap(QRectF{thumb.center().x() - scaled.width() / 2.0,
                                      thumb.center().y() - scaled.height() / 2.0,
                                      scaled.width() * 1.0, scaled.height() * 1.0},
                               pixmap, pixmap.rect());
          } else {
            painter.fillRect(thumb, palette().color(QPalette::Mid));
          }
          painter.restore();

          // The value on the right, and the title and the note in what is left
          const auto& value = line.row->value;
          const auto valueWidth = value.isEmpty() ? 0 : fm.horizontalAdvance(value) + 12;
          const auto textLeft = static_cast<int>(thumb.right()) + 10;
          const auto textWidth = row.right() - textLeft - valueWidth;
          const auto anime = anime::db.item(line.row->id);
          const auto title =
              anime ? QString::fromStdString(anime::preferredTitle(*anime)) : QString{};
          // The title alone is centered when there is no note under it
          const auto lines = line.row->note.isEmpty() ? 1 : 2;
          const auto textTop = row.top() + (row.height() - fm.height() * lines) / 2;
          painter.setPen(palette().color(QPalette::WindowText));
          painter.drawText(QRect{textLeft, textTop, textWidth, fm.height()},
                           Qt::AlignLeft | Qt::AlignVCenter,
                           fm.elidedText(title, Qt::ElideRight, textWidth));
          painter.setPen(muted);
          painter.drawText(QRect{textLeft, textTop + fm.height(), textWidth, fm.height()},
                           Qt::AlignLeft | Qt::AlignVCenter,
                           fm.elidedText(line.row->note, Qt::ElideRight, textWidth));
          if (!value.isEmpty()) {
            painter.drawText(QRect{row.right() - valueWidth, row.top(), valueWidth, row.height()},
                             Qt::AlignRight | Qt::AlignVCenter, value);
          }
          break;
        }
      }
    }
  }

  void mouseMoveEvent(QMouseEvent* event) override {
    const auto index = lineAt(event->position().toPoint());
    if (index == m_hovered) return;
    const auto previous = m_hovered;
    m_hovered = index;
    setCursor(index >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (previous >= 0) update(lineRect(m_lines[previous]).adjusted(-4, 0, 4, 0));
    if (index >= 0) update(lineRect(m_lines[index]).adjusted(-4, 0, 4, 0));
  }

  void leaveEvent(QEvent*) override {
    if (m_hovered >= 0) update(lineRect(m_lines[m_hovered]).adjusted(-4, 0, 4, 0));
    m_hovered = -1;
  }

  void mouseReleaseEvent(QMouseEvent* event) override {
    if (event->button() != Qt::LeftButton) return;
    const auto index = lineAt(event->position().toPoint());
    if (index < 0) return;
    const auto& line = m_lines[index];
    if (line.kind == Line::Kind::More) {
      m_expanded = !m_expanded;
      layoutLines();
    } else if (line.kind == Line::Kind::Row) {
      m_clicked(line.row->id);
    }
  }

private:
  struct Line {
    enum class Kind { Label, Row, More };
    Kind kind = Kind::Row;
    const TimelineRow* row = nullptr;
    QString text;
    int y = 0;
    int height = 0;
  };

  // Sized by the font, so that the rows follow the zoom of the page
  int rowHeight() const {
    return fontMetrics().height() * 3;
  }
  int labelHeight() const {
    return fontMetrics().height() + 10;
  }
  int railWidth() const {
    return fontMetrics().averageCharWidth() * 3;
  }
  bool hasRail() const {
    return std::ranges::any_of(m_groups,
                               [](const TimelineGroup& group) { return !group.label.isEmpty(); });
  }

  QRect lineRect(const Line& line) const {
    return {0, line.y, width(), line.height};
  }

  // Where each label and row goes, up to the limit unless the timeline is open
  void layoutLines() {
    m_lines.clear();
    m_hovered = -1;
    int total = 0;
    for (const auto& group : m_groups) total += static_cast<int>(group.rows.size());

    int y = 0;
    int shown = 0;
    const auto add = [this, &y](Line line) {
      line.y = y;
      y += line.height;
      m_lines.push_back(std::move(line));
    };
    for (const auto& group : m_groups) {
      if (!m_expanded && shown >= m_limit) break;
      if (!group.label.isEmpty()) {
        add({Line::Kind::Label, nullptr, group.label, 0, labelHeight()});
      }
      for (const auto& row : group.rows) {
        if (!m_expanded && shown >= m_limit) break;
        add({Line::Kind::Row, &row, {}, 0, rowHeight()});
        ++shown;
      }
    }
    if (total > m_limit) {
      const auto text = m_expanded ? tr("Show less") : tr("Show all (%1)").arg(total);
      add({Line::Kind::More, nullptr, text, 0, labelHeight()});
    }

    updateGeometry();
    update();
  }

  int lineAt(const QPoint& point) const {
    for (int i = 0; i < std::ssize(m_lines); ++i) {
      const auto& line = m_lines[i];
      if (line.kind != Line::Kind::Label && lineRect(line).contains(point)) return i;
    }
    return -1;
  }

  static QString tr(const char* text) {
    return QCoreApplication::translate("gui::ProfileWidget", text);
  }

  std::vector<TimelineGroup> m_groups;
  std::vector<Line> m_lines;
  int m_limit = 0;
  bool m_expanded = false;
  std::function<void(int)> m_clicked;
  int m_hovered = -1;
};

// The account at the top of the page, over the banner of the profile when the service has one
class ProfileHeader final : public QWidget {
public:
  using QWidget::QWidget;

  void setBanner(const QPixmap& banner) {
    m_banner = banner;
    update();
  }

  bool hasBanner() const {
    return !m_banner.isNull();
  }

protected:
  void paintEvent(QPaintEvent*) override {
    if (m_banner.isNull()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(rect(), 8, 8);
    painter.setClipPath(path);

    // Covers the header and keeps the middle of the picture, as the services show it
    const auto scaled = m_banner.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
    const QRect target{(width() - scaled.width()) / 2, (height() - scaled.height()) / 2,
                       scaled.width(), scaled.height()};
    painter.drawPixmap(target, m_banner);

    // Darker on the left, where the name is, so that it reads on any picture
    QLinearGradient shade{0, 0, static_cast<qreal>(width()), 0};
    shade.setColorAt(0, QColor{0, 0, 0, 170});
    shade.setColorAt(1, QColor{0, 0, 0, 60});
    painter.fillRect(rect(), shade);
  }

private:
  QPixmap m_banner;
};

ProfileWidget::ProfileWidget(QWidget* parent) : PageWidget(parent) {
  const auto container = new QWidget(this);
  const auto containerLayout = new QVBoxLayout(container);
  containerLayout->setContentsMargins(16, 12, 16, 16);
  containerLayout->setSpacing(12);

  // Account
  {
    const auto header = new ProfileHeader(container);
    m_header = header;
    const auto layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    m_avatar = new QLabel(header);
    m_avatar->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_avatar);

    const auto names = new QVBoxLayout();
    m_headerLayout = layout;
    m_names = names;
    names->setSpacing(0);
    m_userName = new QLabel(header);
    m_userName->setTextFormat(Qt::RichText);
    m_userService = new QLabel(header);
    m_userService->setObjectName("profileCaption");
    m_userService->setWordWrap(true);
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
      text->setWordWrap(true);
      layout->addWidget(value);
      layout->addWidget(text);
      m_tileFrames.append(tile);
      return value;
    };
    m_weekEpisodes = addTile(tr("Episodes this week"), tr("Episodes watched in the last 7 days"));
    m_streak = addTile(tr("Days in a row"), tr("Days in a row with at least one episode watched"));
    // The lists below already count what is ready and what airs, so these say something else
    m_weekTime = addTile(tr("Time this week"), tr("Time spent watching in the last 7 days"));
    m_completedYear = addTile(tr("Completed this year"), tr("Anime completed this year"));
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
    // A title over a line rather than a box, which made the page look like a form; see the style
    group->setObjectName("profileSection");
    const auto layout = new QVBoxLayout(group);
    label = createRichLabel(group);
    connect(label, &QLabel::linkActivated, this, &ProfileWidget::openLink);
    layout->addWidget(label);
    m_sections.append(group);
    return group;
  };

  // A timeline under the label, which says why the timeline is empty when it is
  const auto addTimelineGroup = [this, &addListGroup](const QString& title, QLabel*& label,
                                                      Timeline*& timeline) {
    const auto group = addListGroup(title, label);
    timeline = new Timeline(group, kTimelineLimit, [this](const int id) {
      if (const auto anime = anime::db.item(id)) {
        MediaDialog::show(this, MediaDialogPage::Details, *anime);
      }
    });
    group->layout()->addWidget(timeline);
    return group;
  };

  // In this order, each in the shorter of the two columns of a wide window, see `balanceColumns`
  addTimelineGroup(tr("Airing"), m_airing, m_airingTimeline);
  addTimelineGroup(tr("Available Episodes"), m_ready, m_readyTimeline);
  {
    const auto group = addTimelineGroup(tr("Recently Watched"), m_recent, m_recentTimeline);
    m_activityChart = new ActivityChart(group);
    static_cast<QVBoxLayout*>(group->layout())->insertWidget(0, m_activityChart);
  }
  // Upcoming, only when something on the list has not started yet
  m_upcomingGroup = addTimelineGroup(tr("Upcoming"), m_upcoming, m_upcomingTimeline);

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

  const auto refreshAccountIfVisible = [this]() {
    if (isVisible()) refreshAccount();
  };
  connect(&imageProvider, &ImageProvider::userImageChanged, this, refreshAccountIfVisible);
  for (const auto service :
       {static_cast<services::Service*>(services::anilist::Service::instance()),
        static_cast<services::Service*>(services::kitsu::Service::instance()),
        static_cast<services::Service*>(services::myanimelist::Service::instance())}) {
    connect(service, &services::Service::authenticationCompleted, this, refreshAccountIfVisible);
    connect(service, &services::Service::userRefreshed, this, refreshAccountIfVisible);
  }

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
  m_userService->setText(user.isEmpty() ? tr("Add an account to sync your list") : serviceName);
  // As in the Services menu
  m_userButton->setText(user.isEmpty() ? tr("Add account") : tr("Go to my profile"));

  const auto slug = services::serviceSlug(service).toStdString();
  const auto avatarUrl =
      user.isEmpty() ? QString{} : QString::fromStdString(akyuu::accounts.serviceAvatar(slug));
  const auto bannerUrl =
      user.isEmpty() ? QString{} : QString::fromStdString(akyuu::accounts.serviceBanner(slug));
  const auto avatar = imageProvider.loadUserImage(avatarUrl);
  const auto banner = imageProvider.loadUserImage(bannerUrl);

  // The pictures, the styles and the avatar are made again only when something they show changed,
  // not each time the page is shown
  const auto state = u"%1|%2|%3|%4|%5"_s.arg(user)
                         .arg(avatar.cacheKey())
                         .arg(banner.cacheKey())
                         .arg(fontMetrics().height())
                         .arg(palette().color(QPalette::Highlight).name());
  if (state == m_accountState) return;
  m_accountState = state;

  // Over a banner the header is padded like a card, and its text is light on the darkened picture
  m_header->setBanner(banner);
  const auto padding = banner.isNull() ? 0 : 20;
  m_header->layout()->setContentsMargins(padding, padding, padding, padding);
  m_userName->setStyleSheet(banner.isNull() ? QString{} : u"color: white;"_s);
  // Glass over the picture rather than a plain button, which looked pasted on
  m_userButton->setStyleSheet(
      banner.isNull()
          ? QString{}
          : u"QPushButton { color: white; background: rgba(0, 0, 0, 90); border: 1px solid "
            u"rgba(255, 255, 255, 70); border-radius: 8px; padding: 5px 14px; }"
            u"QPushButton:hover { background: rgba(255, 255, 255, 40); }"
            u"QPushButton:pressed { background: rgba(255, 255, 255, 25); }"_s);
  m_userService->setStyleSheet(banner.isNull() ? QString{} : u"color: rgba(255, 255, 255, 200);"_s);

  // The avatar, or the first letter of the name, in a circle sized by the font so that it follows
  // the zoom
  const auto size = fontMetrics().height() * (banner.isNull() ? 3 : 4);
  const auto ratio = devicePixelRatioF();
  QPixmap pixmap(QSize{size, size} * ratio);
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);
  {
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF circle{0, 0, static_cast<qreal>(size), static_cast<qreal>(size)};
    if (!avatar.isNull()) {
      QPainterPath path;
      path.addEllipse(circle);
      painter.setClipPath(path);
      const auto scaled = avatar.size().scaled(QSize{size, size}, Qt::KeepAspectRatioByExpanding);
      painter.drawPixmap(QRect{(size - scaled.width()) / 2, (size - scaled.height()) / 2,
                               scaled.width(), scaled.height()},
                         avatar);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Highlight));
    if (avatar.isNull()) painter.drawEllipse(circle);
    auto font = this->font();
    font.setPixelSize(size / 2);
    font.setWeight(QFont::Weight::DemiBold);
    painter.setFont(font);
    painter.setPen(palette().color(QPalette::HighlightedText));
    if (avatar.isNull()) {
      painter.drawText(QRect{0, 0, size, size}, Qt::AlignCenter,
                       user.isEmpty() ? u"?"_s : user.left(1).toUpper());
    }
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

  // Watched this week, by the length of each episode, or the usual length when it is not known
  {
    constexpr int kUsualEpisodeLength = 24;
    int minutes = 0;
    for (const auto& item : anime::history.items()) {
      if (item.episode <= 0) continue;
      const auto age = localDate(item.time).daysTo(today);
      if (age < 0 || age >= 7) continue;
      const auto anime = anime::db.item(item.anime_id);
      minutes += anime && anime->episode_length > 0 ? anime->episode_length : kUsualEpisodeLength;
    }
    const auto text =
        minutes < 60 ? tr("%1m").arg(minutes) : tr("%1h %2m").arg(minutes / 60).arg(minutes % 60);
    m_weekTime->setText(largeText(this, text, 2.0));
  }

  // Completed this year, by the date on the list
  {
    const auto year = static_cast<unsigned>(today.year());
    const auto completed = std::ranges::count_if(entries, [year](const anime::list::Entry& entry) {
      return entry.status == anime::list::Status::Completed && entry.date_completed.year() == year;
    });
    m_completedYear->setText(tileValue(static_cast<int>(completed)));
  }

  // Recently watched, by day, the History page having the rest
  {
    std::vector<TimelineGroup> groups;
    QDate groupDate;
    int count = 0;
    const auto& items = anime::history.items();
    for (auto it = items.rbegin(); it != items.rend() && count < kRecentLimit; ++it) {
      if (it->episode <= 0) continue;
      const auto anime = anime::db.item(it->anime_id);
      if (!anime) continue;
      const auto time = QDateTime::fromSecsSinceEpoch(it->time).toLocalTime();
      if (groups.empty() || time.date() != groupDate) {
        groupDate = time.date();
        const auto days = groupDate.daysTo(today);
        const auto label = days == 0   ? tr("Today")
                           : days == 1 ? tr("Yesterday")
                           : days < 7  ? QLocale::system().dayName(groupDate.dayOfWeek())
                                       : QLocale::system().toString(groupDate, u"d MMM yyyy"_s);
        groups.push_back({label, {}});
      }
      groups.back().rows.push_back({anime->id, tr("Episode %1").arg(it->episode),
                                    QLocale::system().toString(time.time(), u"HH:mm"_s)});
      ++count;
    }
    m_recent->setText(note(tr("Nothing yet.")));
    m_recent->setVisible(groups.empty());
    m_recentTimeline->setGroups(std::move(groups));
  }

  // Airing next, from what the service said about the anime being watched
  {
    std::vector<const anime::Details*> airing;
    for (const auto& entry : entries) {
      // Planning too, since a show that airs now is the one to start before it piles up
      if (entry.status != anime::list::Status::Watching &&
          entry.status != anime::list::Status::PlanToWatch) {
        continue;
      }
      const auto anime = anime::db.item(entry.anime_id);
      if (anime && anime->next_episode_time > now) airing.push_back(anime);
    }
    std::ranges::sort(airing, {}, &anime::Details::next_episode_time);

    // By day, as a schedule: today, tomorrow, then the names of the days of the week
    std::vector<TimelineGroup> groups;
    QDate groupDate;
    for (const auto anime : airing) {
      const auto time = QDateTime::fromSecsSinceEpoch(anime->next_episode_time).toLocalTime();
      if (groups.empty() || time.date() != groupDate) {
        groupDate = time.date();
        const auto days = today.daysTo(groupDate);
        const auto label = days == 0   ? tr("Today")
                           : days == 1 ? tr("Tomorrow")
                           : days < 7  ? QLocale::system().dayName(groupDate.dayOfWeek())
                                       : QLocale::system().toString(groupDate, u"ddd, d MMM"_s);
        groups.push_back({label, {}});
      }
      QStringList note;
      if (anime->last_aired_episode >= 0 && anime->next_episode_time) {
        note.append(tr("Episode %1").arg(anime->last_aired_episode + 1));
      }
      const auto entry = anime::db.entry(anime->id);
      if (entry && entry->status == anime::list::Status::PlanToWatch) note.append(tr("Planning"));
      groups.back().rows.push_back(
          {anime->id, note.join(u", "_s), QLocale::system().toString(time.time(), u"HH:mm"_s)});
    }
    m_airing->setText(u"<p style=\"margin: 0;\">%1 <a href=\"akyuu:seasons\">%2</a></p>"_s.arg(
        tr("Nothing on your list is airing.").toHtmlEscaped(),
        tr("See this season").toHtmlEscaped()));
    m_airing->setVisible(groups.empty());
    m_airingTimeline->setGroups(std::move(groups));
  }

  // Upcoming, from Watching and Planning, by when they start
  {
    std::vector<const anime::Details*> upcoming;
    for (const auto& entry : entries) {
      if (entry.status != anime::list::Status::Watching &&
          entry.status != anime::list::Status::PlanToWatch) {
        continue;
      }
      const auto anime = anime::db.item(entry.anime_id);
      if (anime && anime->status == anime::Status::NotYetAired) upcoming.push_back(anime);
    }
    // A known date first, the month and the year for what is not known yet
    const auto key = [](const anime::Details* anime) {
      const auto& date = anime->date_started;
      const auto year = date.year() ? date.year() : 9999u;
      const auto month = date.month() ? date.month() : 13u;
      return std::pair{year, month};
    };
    std::ranges::sort(upcoming, {}, key);

    // By the date they start, as much of it as is known
    std::vector<TimelineGroup> groups;
    for (const auto anime : upcoming) {
      const auto label = formatFuzzyDate(anime->date_started, tr("TBA"));
      if (groups.empty() || groups.back().label != label) groups.push_back({label, {}});
      const auto type = anime->type == anime::Type::Unknown ? QString{} : formatType(anime->type);
      groups.back().rows.push_back({anime->id, type, {}});
    }
    m_upcoming->hide();
    m_upcomingGroup->setVisible(!groups.empty());
    m_upcomingTimeline->setGroups(std::move(groups));
  }

  // Ready to watch, from the library folders
  {
    struct Ready {
      const anime::Details* anime;
      const anime::list::Entry* entry;
      std::vector<track::EpisodeRange> ranges;
    };
    std::vector<Ready> ready;
    for (const auto& entry : entries) {
      using enum anime::list::Status;
      if (entry.status == Completed || entry.status == Dropped) continue;
      const auto anime = anime::db.item(entry.anime_id);
      if (!anime) continue;
      auto ranges = unwatchedRanges(entry);
      if (ranges.empty()) continue;
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

    std::vector<TimelineGroup> groups;
    if (scanned && !ready.empty()) {
      groups.push_back({});
      for (const auto& item : ready) {
        QStringList ranges;
        int count = 0;
        for (const auto& [first, last] : item.ranges) {
          ranges.append(formatEpisodeRange(first, last));
          count += last - first + 1;
        }
        groups.back().rows.push_back(
            {item.anime->id, ranges.join(u", "_s), tr("%n new", nullptr, count)});
      }
    }
    m_ready->setVisible(groups.empty());
    m_readyTimeline->setGroups(std::move(groups));
    // What to do about an empty list, when there is something to do
    if (akyuu::settings.libraryFolders().empty()) {
      m_ready->setText(
          u"<p style=\"margin: 0;\">%1 <a href=\"akyuu:library-settings\">%2</a></p>"_s.arg(
              tr("No library folders.").toHtmlEscaped(), tr("Add folders").toHtmlEscaped()));
    } else if (!scanned) {
      // Scanned as soon as the page is shown, see `showEvent`
      m_ready->setText(note(tr("Scanning...")));
    } else {
      m_ready->setText(note(tr("No new episodes in library folders.")));
    }
  }

  // The sections changed height, and may now fit the columns better the other way
  if (m_columnCount) balanceColumns();
}

void ProfileWidget::openLink(const QString& link) {
  const QUrl url{link};
  if (url.scheme() != u"akyuu"_s) return;
  if (url.path() == u"seasons"_s) {
    mainWindow()->navigateTo(MainWindowPage::Seasons);
    return;
  }
  if (url.path() == u"library-settings"_s) {
    SettingsDialog::show(this, SettingsPageId::Library);
  }
}

// Side by side when both columns have room for their lists, one under the other otherwise
void ProfileWidget::arrangeColumns() {
  const auto available = m_scrollArea ? m_scrollArea->viewport()->width() : width();
  // The button of the account goes under its name only when the row has no room for it
  {
    const auto margins = m_headerLayout->contentsMargins();
    const auto names =
        std::max(m_userName->sizeHint().width(),
                 m_userService->fontMetrics().horizontalAdvance(m_userService->text()));
    const auto needed = 32 + margins.left() + margins.right() + m_avatar->width() + names +
                        m_userButton->sizeHint().width() + m_headerLayout->spacing() * 2;
    const auto under = available < needed;
    if (under != m_buttonUnder || !m_buttonPlaced) {
      if (m_buttonPlaced) {
        if (m_buttonUnder) {
          m_names->removeWidget(m_userButton);
          // The spacing that was put above the button
          delete m_names->takeAt(m_names->count() - 1);
        } else {
          m_headerLayout->removeWidget(m_userButton);
        }
      }
      if (under) {
        m_names->addSpacing(6);
        m_names->addWidget(m_userButton, 0, Qt::AlignLeft);
      } else {
        m_headerLayout->addWidget(m_userButton, 0, Qt::AlignVCenter);
      }
      m_buttonUnder = under;
      m_buttonPlaced = true;
    }
  }

  const auto columnCount = available >= fontMetrics().averageCharWidth() * kTwoColumnWidth ? 2 : 1;
  if (columnCount == m_columnCount) return;
  m_columnCount = columnCount;

  const auto tilesPerRow = columnCount == 2 ? 4 : 2;
  for (int i = 0; i < m_tileFrames.size(); ++i) {
    m_tiles->addWidget(m_tileFrames[i], i / tilesPerRow, i % tilesPerRow);
  }
  for (int i = 0; i < 4; ++i) m_tiles->setColumnStretch(i, i < tilesPerRow ? 1 : 0);

  m_columns->setStretch(1, columnCount == 2 ? 1 : 0);
  balanceColumns();
}

// Each section goes, in order, into the column that is shorter so far, so that neither ends long
// before the other. A narrow window has them all in one.
void ProfileWidget::balanceColumns() {
  for (const auto layout : {m_leftColumn, m_rightColumn}) {
    while (layout->count()) {
      const auto item = layout->takeAt(0);
      if (item->widget()) item->widget()->setParent(m_container);
      delete item;
    }
  }

  int heights[2] = {0, 0};
  for (const auto section : m_sections) {
    const auto column = m_columnCount == 2 && heights[1] < heights[0] ? 1 : 0;
    (column == 0 ? m_leftColumn : m_rightColumn)->addWidget(section);
    if (!section->isHidden()) heights[column] += section->sizeHint().height();
  }
  m_leftColumn->addStretch();
  m_rightColumn->addStretch();
}

void ProfileWidget::hideEvent(QHideEvent* event) {
  m_activityTimer->stop();
  PageWidget::hideEvent(event);
}

void ProfileWidget::showEvent(QShowEvent* event) {
  // The library is scanned once when it has not been yet, after the page is drawn, so that what is
  // available shows without asking. The folders are watched afterwards, which keeps it current.
  if (!track::library()->hasScanned() && !akyuu::settings.libraryFolders().empty()) {
    QTimer::singleShot(0, this, []() {
      if (!track::library()->hasScanned()) track::library()->scan();
    });
  }
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
