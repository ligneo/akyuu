/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
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

#include "feed_aggregator.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QRestReply>
#include <QUrl>
#include <algorithm>
#include <map>
#include <set>

#include "base/log.hpp"
#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "media/anime_utils.hpp"
#include "akyuu/path.hpp"
#include "akyuu/settings.hpp"
#include "track/feed_archive.hpp"
#include "track/feed_filter_manager.hpp"

namespace track {

Aggregator::Aggregator(QObject* parent) : QObject(parent) {
  timer_.setSingleShot(false);
  connect(&timer_, &QTimer::timeout, this, [this]() { fetch({}, true); });
  applyAutoCheckSettings();
}

void Aggregator::applyAutoCheckSettings() {
  const auto interval = taiga::settings.torrentAutoCheckInterval();

  if (!taiga::settings.torrentAutoCheckEnabled() || interval.count() < 1) {
    timer_.stop();
    return;
  }

  timer_.start(interval);
}

std::chrono::milliseconds Aggregator::timeUntilNextCheck() const {
  if (!timer_.isActive()) return std::chrono::milliseconds::zero();
  return std::chrono::milliseconds{timer_.remainingTime()};
}

void Aggregator::search(const QString& title) {
  auto url = QString::fromStdString(taiga::settings.torrentSearchUrl());

  // v1 substitutes the encoded title, so that a title with spaces or symbols stays a valid URL.
  url.replace(u"%title%"_s, QString::fromUtf8(QUrl::toPercentEncoding(title)));

  fetch(url);
}

void Aggregator::fetch(const QString& requestedUrl, const bool automatic) {
  if (fetching_) return;

  const auto url = !requestedUrl.isEmpty()
                       ? requestedUrl
                       : QString::fromStdString(taiga::settings.torrentDiscoveryUrl());

  if (url.isEmpty()) {
    emit errorOccurred(tr("No feed address is set."));
    return;
  }

  fetching_ = true;
  emit fetchingChanged(true);

  QNetworkRequest request{QUrl{url}};
  request.setHeaders(taiga::NetworkAccessManager::commonHeaders());

  manager_.get(request, this, [this, automatic](QRestReply& reply) {
    fetching_ = false;
    emit fetchingChanged(false);

    if (!reply.isHttpStatusSuccess() || reply.hasError()) {
      emit errorOccurred(reply.errorString());
      return;
    }

    const auto feed = parseFeed(QString::fromUtf8(reply.readBody()));

    if (!feed) {
      emit errorOccurred(tr("Could not read the feed."));
      return;
    }

    feed_ = *feed;
    examineFeed(feed_);
    filterManager.filter(feed_);

    // Anything already downloaded or thrown away is dropped, as in v1's `FilterArchived()`.
    for (auto& item : feed_.items) {
      if (item.isDiscarded()) continue;
      if (archive.contains(QString::fromStdString(item.title))) {
        item.state = FeedItemState::DiscardedNormal;
      }
    }

    emit feedChanged();

    // Only an automatic check acts on its own, so that refreshing by hand stays quiet. As in v1.
    if (automatic) {
      if (taiga::settings.torrentNotifyNewEpisodes()) {
        if (const auto lines = newEpisodeLines(); !lines.isEmpty()) {
          emit newEpisodesFound(lines);
        }
      } else if (taiga::settings.torrentDownloadNewEpisodes()) {
        // The filters decide what is selected. With them off, this would download everything.
        if (taiga::settings.torrentFilterEnabled()) downloadSelected();
      }
    }
  });
}

namespace {

// v1 passes the download folder to the client it recognizes, each with its own flags. The names
// below are the ones that exist on Linux; v1's PicoTorrent and uTorrent are Windows-only.
QStringList clientArguments(const QString& command, const QString& target, const QString& folder) {
  const auto name = QFileInfo(command).fileName();
  const auto matches = [&name](const char* client) {
    return name.contains(QLatin1StringView{client}, Qt::CaseInsensitive);
  };

  if (matches("aria2c")) {
    if (folder.isEmpty()) return {target};
    return {u"--dir=%1"_s.arg(folder), target};
  }
  if (matches("deluge-console")) {
    if (folder.isEmpty()) return {u"add"_s, target};
    return {u"add"_s, u"-p"_s, folder, target};
  }
  if (matches("transmission-remote")) {
    if (folder.isEmpty()) return {u"-a"_s, target};
    return {u"-a"_s, target, u"-w"_s, folder};
  }
  if (matches("qbittorrent")) {
    if (folder.isEmpty()) return {u"--skip-dialog=true"_s, target};
    return {u"--skip-dialog=true"_s, u"--save-path=%1"_s.arg(folder), target};
  }

  return {target};
}

// A feed title becomes a file name, so it cannot carry a separator or the characters a file
// system refuses.
QString sanitizedFileName(QString title) {
  static const QRegularExpression invalid{uR"([/\\:*?"<>|])"_s};
  title.replace(invalid, u"_"_s);
  return title.trimmed().left(200);
}

// v1's download folder: the anime's own folder, or else the fallback location, optionally with a
// subfolder named after the anime that then becomes the anime's folder.
QString downloadFolder(const track::Episode& episode) {
  if (!taiga::settings.torrentDownloadUseAnimeFolder()) return {};

  const auto animeId = episode.animeId();

  if (const auto settings = anime::db.settings(animeId)) {
    const auto folder = QString::fromStdString(settings->folder);
    if (!folder.isEmpty() && QFileInfo(folder).isDir()) return folder;
  }

  auto folder = QString::fromStdString(taiga::settings.torrentDownloadLocation());
  if (folder.isEmpty() || !taiga::settings.torrentDownloadCreateSubfolder()) return folder;

  const auto anime = anime::db.item(animeId);
  const auto title = anime ? QString::fromStdString(anime::preferredTitle(*anime))
                           : QString::fromStdString(episode.element(anitomy::ElementKind::Title));
  const auto subfolder = sanitizedFileName(title);
  if (subfolder.isEmpty()) return folder;

  folder = QDir(folder).filePath(subfolder);

  if (QDir().mkpath(folder) && anime) {
    auto settings =
        anime::db.settings(animeId) ? *anime::db.settings(animeId) : anime::Settings{.id = animeId};
    settings.folder = folder.toStdString();
    anime::db.updateSettings(settings);
  }

  return folder;
}

}  // namespace

// v1 downloads the marked items one after another, ordered by the queue settings.
void Aggregator::downloadSelected() {
  std::vector<const FeedItem*> items;

  for (const auto& item : feed_.items) {
    if (item.state == FeedItemState::Selected) items.push_back(&item);
  }

  if (items.empty()) {
    emit errorOccurred(tr("No torrents are marked for download."));
    return;
  }

  const auto sortBy = QString::fromStdString(taiga::settings.torrentDownloadSortBy());
  const auto descending =
      taiga::settings.torrentDownloadSortOrder() == Qt::SortOrder::DescendingOrder;

  std::ranges::stable_sort(items, [&sortBy, descending](const FeedItem* a, const FeedItem* b) {
    // Items of the same anime stay together, as in v1.
    if (a->episode.animeId() != b->episode.animeId()) {
      return a->episode.animeId() < b->episode.animeId();
    }

    if (sortBy == u"releaseDate") {
      const auto first = parseDate(*a);
      const auto second = parseDate(*b);
      return descending ? second < first : first < second;
    }

    const auto number = [](const FeedItem* item) {
      const auto range = item->episode.episodeNumberRange();
      return range ? range->second : 0;
    };

    return descending ? number(b) < number(a) : number(a) < number(b);
  });

  // A copy is needed: downloading changes the feed the pointers come from.
  std::vector<FeedItem> queue;
  for (const auto* item : items) queue.push_back(*item);

  for (const auto& item : queue) download(item);
}

// v1's "Discard all", which throws the marked items away and remembers them.
void Aggregator::discardSelected() {
  int count = 0;

  for (auto& item : feed_.items) {
    if (item.state != FeedItemState::Selected) continue;
    item.state = FeedItemState::DiscardedNormal;
    archive.add(QString::fromStdString(item.title));
    ++count;
  }

  if (count) emit feedChanged();
}

// Hands a torrent to a BitTorrent client, remembering it so that the next check does not offer it
// again. A magnet link goes straight to the client; anything else is fetched to a file first.
void Aggregator::download(const FeedItem& item) {
  const auto title = QString::fromStdString(item.title);

  const auto handOff = [this, title, episode = item.episode](const QString& target) {
    // The archive is written before the hand-off on purpose: if launching the client fails, the
    // item must still not come back on the next check, exactly as in v1.
    archive.add(title);

    for (auto& feedItem : feed_.items) {
      if (feedItem.title == title.toStdString()) feedItem.state = FeedItemState::DiscardedNormal;
    }

    emit feedChanged();

    if (!taiga::settings.torrentDownloadOpen()) {
      emit downloadFinished(title);
      return;
    }

    const auto mode = QString::fromStdString(taiga::settings.torrentDownloadAppMode());
    const auto command = QString::fromStdString(taiga::settings.torrentDownloadAppPath());

    bool started = false;

    if (mode != u"default" && !command.isEmpty()) {
      started = QProcess::startDetached(command,
                                        clientArguments(command, target, downloadFolder(episode)));
    } else {
      const auto url = target.startsWith(u"magnet:") ? QUrl{target} : QUrl::fromLocalFile(target);
      started = QDesktopServices::openUrl(url);
    }

    if (!started) {
      emit errorOccurred(tr("Could not open \"%1\" with a BitTorrent client.").arg(title));
      return;
    }

    emit downloadFinished(title);
  };

  const auto magnet = QString::fromStdString(item.magnet_link);

  if (!magnet.isEmpty() && taiga::settings.torrentDownloadUseMagnet()) {
    handOff(magnet);
    return;
  }

  const auto link = QString::fromStdString(item.link);

  if (link.startsWith(u"magnet:")) {
    handOff(link);
    return;
  }

  if (link.isEmpty()) {
    if (magnet.isEmpty()) {
      emit errorOccurred(tr("\"%1\" has nothing to download.").arg(title));
      return;
    }
    handOff(magnet);
    return;
  }

  auto directory = QString::fromStdString(taiga::settings.torrentDownloadFileLocation());
  if (directory.isEmpty()) {
    directory = u"%1/torrents"_s.arg(QString::fromStdString(taiga::get_data_path()));
  }

  if (!QDir().mkpath(directory)) {
    emit errorOccurred(tr("Could not create the folder for torrent files."));
    return;
  }

  const auto path = u"%1/%2.torrent"_s.arg(directory).arg(sanitizedFileName(title));

  QNetworkRequest request{QUrl{link}};
  request.setHeaders(taiga::NetworkAccessManager::commonHeaders());
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);

  manager_.get(request, this, [this, title, path, magnet, handOff](QRestReply& reply) {
    if (!reply.isHttpStatusSuccess() || reply.hasError()) {
      // Some providers only serve the file to a browser, so a magnet link is the way out.
      if (!magnet.isEmpty()) {
        handOff(magnet);
        return;
      }
      emit errorOccurred(tr("Could not download \"%1\": %2").arg(title).arg(reply.errorString()));
      return;
    }

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly) || file.write(reply.readBody()) < 0) {
      emit errorOccurred(tr("Could not save the torrent file for \"%1\".").arg(title));
      return;
    }

    file.close();
    handOff(path);
  });
}

// One line per anime, with the episode numbers that are new. v1 groups the same way.
QStringList Aggregator::newEpisodeLines() const {
  std::map<QString, std::set<int>> episodes;

  for (const auto& item : feed_.items) {
    if (!item.new_episode) continue;

    const auto anime = anime::db.item(item.episode.animeId());
    const auto title =
        anime ? QString::fromStdString(anime::preferredTitle(*anime))
              : QString::fromStdString(item.episode.element(anitomy::ElementKind::Title));

    if (const auto range = item.episode.episodeNumberRange()) {
      episodes[title].insert(range->second);
    }
  }

  QStringList lines;

  for (const auto& [title, numbers] : episodes) {
    QStringList parts;
    for (const auto number : numbers) parts << u"#%1"_s.arg(number);
    lines << u"\u00BB %1 %2"_s.arg(title, parts.join(u' '));
  }

  return lines;
}

bool Aggregator::fetching() const {
  return fetching_;
}

const Feed& Aggregator::feed() const {
  return feed_;
}

}  // namespace track
