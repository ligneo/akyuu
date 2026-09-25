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

#include "now_playing_page.hpp"

#include <QDate>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QTextBrowser>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>

#include "base/string.hpp"
#include "gui/common/poster_widget.hpp"
#include "gui/main/main_window.hpp"
#include "gui/media/media_dialog.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/image_provider.hpp"
#include "media/anime_db.hpp"
#include "media/anime_history.hpp"
#include "media/anime_list_utils.hpp"
#include "media/anime_season.hpp"
#include "media/anime_utils.hpp"
#include "sync/service.hpp"
#include "track/library.hpp"
#include "track/media.hpp"
#include "track/play.hpp"
#include "ui_main_window.h"

namespace gui {

namespace {

using namespace Qt::StringLiterals;

constexpr int kDayLimit = 7;
constexpr int kContinueWatchingLimit = 20;

QString escaped(const std::string& text) {
  return QString::fromStdString(text).toHtmlEscaped();
}

QString link(const QString& href, const QString& text) {
  return u"<a href=\"%1\">%2</a>"_s.arg(href, text);
}

QString heading(const QString& text) {
  return u"<h4 style=\"margin-top: 12px; margin-bottom: 0;\">%1</h4><hr>"_s.arg(text);
}

QDate toDate(const base::FuzzyDate& date) {
  if (!date.year() || !date.month() || !date.day()) return {};
  return QDate(date.year(), date.month(), date.day());
}

bool isWatching(const ListEntry& entry) {
  return entry.status == anime::list::Status::Watching || entry.rewatching;
}

}  // namespace

NowPlayingPage::NowPlayingPage(QWidget* parent)
    : QWidget(parent), m_poster(new PosterWidget(this)), m_browser(new QTextBrowser(this)) {
  const auto layout = new QHBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(16);

  m_poster->setFixedSize(200, 285);
  m_poster->setCornerRadius(4);
  layout->addWidget(m_poster, 0, Qt::AlignTop);

  m_browser->setFrameShape(QFrame::NoFrame);
  m_browser->setOpenLinks(false);
  m_browser->viewport()->setAutoFillBackground(false);
  layout->addWidget(m_browser, 1);

  connect(m_browser, &QTextBrowser::anchorClicked, this, &NowPlayingPage::openLink);
  connect(m_poster, &PosterWidget::clicked, this, [this]() {
    if (const auto anime = anime::db.item(m_animeId)) {
      MediaDialog::show(this, MediaDialogPage::Details, *anime);
    }
  });

  connect(track::media::detection(), &track::media::Detection::currentEpisodeChanged, this,
          &NowPlayingPage::refresh);
  connect(&anime::history, &anime::History::changed, this, &NowPlayingPage::refresh);
  connect(&imageProvider, &ImageProvider::posterChanged, this, [this](const int id) {
    if (id == m_animeId) refresh();
  });
}

void NowPlayingPage::showEvent(QShowEvent* event) {
  refresh();
  QWidget::showEvent(event);
}

void NowPlayingPage::refresh() {
  if (!isVisible()) return;

  const auto episode = track::media::detection()->getCurrentEpisode();
  m_animeId = episode ? episode->animeId() : 0;

  const auto anime = anime::db.item(m_animeId);
  m_poster->setVisible(anime != nullptr);
  if (anime) m_poster->setPixmap(imageProvider.loadPoster(anime->id));

  m_browser->setHtml(episode ? playingHtml() : idleHtml());
}

QString NowPlayingPage::playingHtml() const {
  const auto episode = track::media::detection()->getCurrentEpisode();
  const auto anime = anime::db.item(m_animeId);

  // v1 asked for help here; searching is the help we can offer.
  if (!anime) {
    const auto title = QString::fromStdString(episode->element(anitomy::ElementKind::Title));
    return u"<h2>%1</h2><p>%2</p><p>%3</p>"_s.arg(
        title.toHtmlEscaped(), tr("Akyuu was unable to identify this title, and needs your help."),
        link(u"akyuu:search"_s, tr("Search for this title")));
  }

  QString html =
      u"<h2>%1</h2>"_s.arg(link(u"akyuu:info"_s, escaped(anime::preferredTitle(*anime))));

  auto playing = tr("Now playing: Episode %1")
                     .arg(formatEpisodeNumbers(episode->elements(anitomy::ElementKind::Episode)));
  if (episode->contains(anitomy::ElementKind::ReleaseGroup)) {
    playing +=
        u" "_s + tr("by %1").arg(escaped(episode->element(anitomy::ElementKind::ReleaseGroup)));
  }

  const auto entry = anime::db.entry(anime->id);
  QStringList links{anime::list::isInList(entry) ? link(u"akyuu:edit"_s, tr("Edit"))
                                                 : link(u"akyuu:edit"_s, tr("Add to list")),
                    link(u"akyuu:share"_s, tr("Share"))};
  if (const auto next = track::nextEpisodeNumber(anime->id);
      next && (anime->episode_count <= 0 || *next <= anime->episode_count)) {
    links.append(link(u"akyuu:next"_s, tr("Watch next episode")));
  }
  html += u"<p>%1<br>%2</p>"_s.arg(playing, links.join(u" • "_s));

  // Alternative titles
  QStringList titles;
  for (const auto& title : {anime->titles.english, anime->titles.japanese}) {
    if (!title.empty() && title != anime::preferredTitle(*anime)) titles.append(escaped(title));
  }
  for (const auto& synonym : anime->titles.synonyms) titles.append(escaped(synonym));
  if (!titles.isEmpty()) {
    html += heading(tr("Alternative titles")) + u"<p>%1</p>"_s.arg(titles.join(u", "_s));
  }

  // Details
  const bool hasStudios = !anime->studios.empty();
  const QList<QPair<QString, QString>> rows{
      {tr("Type:"), formatType(anime->type)},
      {tr("Episodes:"), formatNumber(anime->episode_count, "?")},
      {tr("Status:"), formatStatus(anime->status)},
      {tr("Season:"), formatSeason(anime::Season(anime->date_started))},
      {tr("Genres:"), joinStrings(anime->genres)},
      {hasStudios ? tr("Studios:") : tr("Producers:"),
       joinStrings(hasStudios ? anime->studios : anime->producers)},
      {tr("Score:"), anime->score > 0 ? formatScore(anime->score) : u"?"_s},
  };
  QString table = u"<table cellspacing=\"0\" cellpadding=\"2\">"_s;
  for (const auto& [label, value] : rows) {
    table += u"<tr><td style=\"padding-right: 24px;\">%1</td><td>%2</td></tr>"_s.arg(
        label, value.toHtmlEscaped());
  }
  table += u"</table>"_s;
  html += heading(tr("Details")) + table;

  // Synopsis
  if (!anime->synopsis.empty()) {
    auto synopsis = QString::fromStdString(anime->synopsis);
    synopsis.replace(u"<br>"_s, u"\n"_s);
    removeHtmlTags(synopsis);
    html += heading(tr("Synopsis")) +
            u"<p>%1</p>"_s.arg(synopsis.trimmed().toHtmlEscaped().replace(u"\n"_s, u"<br>"_s));
  }

  return html;
}

// v1 `AnimeDialog::Refresh` with nothing playing (`dlg_anime_info.cpp`)
QString NowPlayingPage::idleHtml() const {
  const auto today = QDate::currentDate();
  const auto now = QDateTime::currentSecsSinceEpoch();
  QString html;

  // Continue watching: what was watched last, then the rest of the list, as long as the next
  // episode is waiting in the library folders.
  QList<int> ids;
  const auto consider = [&ids](const int id) {
    if (ids.contains(id)) return;
    const auto anime = anime::db.item(id);
    const auto entry = anime::db.entry(id);
    if (!anime || !entry || !isWatching(*entry)) return;
    if (hasNewEpisode(*anime, entry)) ids.append(id);
  };
  auto history = anime::history.items();
  std::ranges::sort(history, [](const auto& a, const auto& b) { return a.time > b.time; });
  for (const auto& item : history) consider(item.anime_id);
  QList<ListEntry> entries = anime::db.entries().values();
  std::ranges::sort(entries,
                    [](const auto& a, const auto& b) { return a.last_updated > b.last_updated; });
  for (const auto& entry : entries) consider(entry.anime_id);

  html += u"<h3>%1</h3>"_s.arg(tr("Continue Watching"));
  if (ids.isEmpty()) {
    html += u"<p>%1</p>"_s.arg(tr("%1 to see recently watched anime. Or how about you %2?")
                                   .arg(link(u"akyuu:scan"_s, tr("Scan available episodes")),
                                        link(u"akyuu:random"_s, tr("try a random one"))));
  } else {
    html += u"<ul>"_s;
    for (const auto id : ids.first(std::min<qsizetype>(ids.size(), kContinueWatchingLimit))) {
      const auto anime = anime::db.item(id);
      const auto entry = anime::db.entry(id);
      html += u"<li>%1</li>"_s.arg(link(u"akyuu:play?id=%1"_s.arg(id),
                                        u"%1 #%2"_s.arg(escaped(anime::preferredTitle(*anime)))
                                            .arg(entry->watched_episodes + 1)));
    }
    html += u"</ul>"_s;
  }

  // Watched last week
  const auto watchedLastWeek = std::ranges::count_if(history, [now](const auto& item) {
    return item.episode > 0 && now - item.time <= kDayLimit * 24 * 60 * 60;
  });
  if (watchedLastWeek > 0) {
    html += u"<p>%1</p>"_s.arg(
        watchedLastWeek == 1 ? tr("You've watched 1 episode last week.")
                             : tr("You've watched %1 episodes last week.").arg(watchedLastWeek));
  }

  // Available episodes
  const auto available = std::ranges::count_if(entries, [](const ListEntry& entry) {
    const auto anime = anime::db.item(entry.anime_id);
    return anime && hasNewEpisode(*anime, &entry);
  });
  if (available > 0) {
    html += u"<p>%1</p>"_s.arg(
        available == 1 ? tr("There is at least 1 new episode available in library folders.")
                       : tr("There are at least %1 new episodes available in library folders.")
                             .arg(available));
  }

  // Airing times of what is planned
  QStringList started, finished, upcoming;
  for (const auto& entry : entries) {
    if (entry.status != anime::list::Status::PlanToWatch) continue;
    const auto anime = anime::db.item(entry.anime_id);
    if (!anime) continue;
    const auto title =
        link(u"akyuu:info?id=%1"_s.arg(anime->id), escaped(anime::preferredTitle(*anime)));
    if (const auto start = toDate(anime->date_started); start.isValid()) {
      const auto days = start.daysTo(today);
      if (days > 0 && days <= kDayLimit) {
        started.append(title);
        continue;
      }
      if (days < 0 && -days <= kDayLimit) {
        upcoming.append(title);
        continue;
      }
    }
    if (const auto end = toDate(anime->date_finished); end.isValid()) {
      const auto days = end.daysTo(today);
      if (days > 0 && days <= kDayLimit) finished.append(title);
    }
  }
  const auto airing = [&html](const QString& title, const QStringList& items) {
    if (items.isEmpty()) return;
    html += u"<h3>%1</h3><p>%2</p>"_s.arg(title, items.join(u"  •  "_s));
  };
  airing(tr("Recently Started Airing"), started);
  airing(tr("Recently Finished Airing"), finished);
  airing(tr("Upcoming"), upcoming);

  return html;
}

void NowPlayingPage::openLink(const QUrl& url) {
  const auto action = url.path();
  const auto id = QUrlQuery(url).queryItemValue(u"id"_s).toInt();
  const auto anime = anime::db.item(id ? id : m_animeId);

  if (action == u"info"_s) {
    if (anime) MediaDialog::show(this, MediaDialogPage::Details, *anime);
  } else if (action == u"edit"_s) {
    if (anime) MediaDialog::show(this, MediaDialogPage::List, *anime);
  } else if (action == u"share"_s) {
    QMenu menu(this);
    menu.addAction(u"Discord"_s, this, []() {
      mainWindow()->announceCurrentEpisode(MainWindow::ShareChannel::Discord);
    });
    menu.addAction(u"HTTP"_s, this,
                   []() { mainWindow()->announceCurrentEpisode(MainWindow::ShareChannel::Http); });
#ifdef Q_OS_LINUX
    menu.addAction(u"IRC"_s, this,
                   []() { mainWindow()->announceCurrentEpisode(MainWindow::ShareChannel::Irc); });
#endif
    menu.exec(QCursor::pos());
  } else if (action == u"next"_s) {
    if (anime) {
      if (const auto next = track::nextEpisodeNumber(anime->id)) {
        track::playEpisode(anime->id, *next);
      }
    }
  } else if (action == u"play"_s) {
    if (anime) {
      if (const auto next = track::nextEpisodeNumber(anime->id)) {
        track::playEpisode(anime->id, *next);
      }
    }
  } else if (action == u"scan"_s) {
    mainWindow()->ui()->actionScanAvailableEpisodes->trigger();
  } else if (action == u"random"_s) {
    mainWindow()->ui()->actionPlayRandomAnime->trigger();
  } else if (action == u"search"_s) {
    const auto episode = track::media::detection()->getCurrentEpisode();
    mainWindow()->navigateTo(MainWindowPage::Search);
    if (episode) {
      mainWindow()->searchBox()->setText(
          QString::fromStdString(episode->element(anitomy::ElementKind::Title)));
    }
  }
}

}  // namespace gui
