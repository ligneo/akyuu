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

#include "feed.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QRegularExpression>
#include <QUrl>
#include <map>

#include "base/string.hpp"
#include "media/anime_db.hpp"
#include "track/recognition.hpp"
#include "track/recognition_validate.hpp"

namespace track {

namespace {

// Returns the text between two delimiters, which is how most providers publish the file size.
QString between(const QString& text, const QString& from, const QString& to) {
  const auto first = text.indexOf(from);
  if (first < 0) return {};

  const auto begin = first + from.size();
  const auto last = to.isEmpty() ? -1 : text.indexOf(to, begin);

  return last < 0 ? text.mid(begin) : text.mid(begin, last - begin);
}

std::optional<int> namespaceInt(const FeedItem& item, const std::string& name) {
  const auto it = item.namespace_elements.find(name);
  if (it == item.namespace_elements.end()) return std::nullopt;
  return QString::fromStdString(it->second).toInt();
}

quint64 namespaceSize(const FeedItem& item, const std::string& name) {
  const auto it = item.namespace_elements.find(name);
  if (it == item.namespace_elements.end()) return 0;
  return parseSizeString(QString::fromStdString(it->second));
}

void parseMagnetLink(FeedItem& item) {
  if (item.enclosure.url.starts_with("magnet:")) {
    item.magnet_link = item.enclosure.url;
    return;
  }

  if (item.link.starts_with("magnet:")) {
    item.magnet_link = item.link;
    return;
  }

  const auto description = QString::fromStdString(item.description);
  if (const auto link = between(description, u"<a href=\"magnet:?"_s, u"\">"_s); !link.isEmpty()) {
    item.magnet_link = u"magnet:?%1"_s.arg(link).toStdString();
  }
}

void parseItemFromSource(const FeedSource source, FeedItem& item) {
  const auto description = QString::fromStdString(item.description);

  switch (source) {
    case FeedSource::Acgnx: {
      const auto parts = description.split(u" | "_s);
      if (parts.size() > 2) item.file_size = parseSizeString(parts.at(2));
      parseMagnetLink(item);
      break;
    }

    case FeedSource::AniDex:
      item.file_size = parseSizeString(between(description, u"Size: "_s, u" |"_s));
      parseMagnetLink(item);
      break;

    case FeedSource::AnimeTosho:
      if (item.link.contains("/view/") && !item.enclosure.url.empty()) {
        item.link = item.enclosure.url;
      }
      item.info_link = item.guid.value;
      item.file_size =
          parseSizeString(between(description, u"<strong>Total Size</strong>: "_s, u"<"_s));
      parseMagnetLink(item);
      break;

    case FeedSource::Minglong:
      item.file_size = parseSizeString(between(description, u"Size: "_s, u"<"_s));
      break;

    case FeedSource::NyaaPantsu:
      item.info_link = item.guid.value;
      item.file_size = QString::fromStdString(item.enclosure.length).toULongLong();
      break;

    case FeedSource::NyaaSi:
      item.info_link = item.guid.value;
      item.file_size = namespaceSize(item, "nyaa:size");
      item.seeders = namespaceInt(item, "nyaa:seeders");
      item.leechers = namespaceInt(item, "nyaa:leechers");
      item.downloads = namespaceInt(item, "nyaa:downloads");
      break;

    case FeedSource::SubsPlease:
      item.file_size = namespaceSize(item, "subsplease:size");
      parseMagnetLink(item);
      break;

    case FeedSource::TokyoToshokan:
      item.file_size = parseSizeString(between(description, u"Size: "_s, u"<"_s));
      parseMagnetLink(item);
      item.description = between(description, u"Comment: "_s, {}).toStdString();
      item.info_link = item.guid.value;
      break;

    default:
      parseMagnetLink(item);
      break;
  }
}

// A volume, or a release marked as such, covers more than one episode.
bool isBatchRelease(const Episode& episode) {
  if (episode.contains(anitomy::ElementKind::Volume)) return true;

  for (const auto& value : episode.elements(anitomy::ElementKind::ReleaseInformation)) {
    if (compareStrings(value, "batch", Qt::CaseInsensitive) == 0) return true;
  }

  return false;
}

// An item is new when it belongs to an anime on the list and goes beyond what has been watched.
// This is v1's `MarkNewEpisodes()`.
bool isNewEpisode(const FeedItem& item) {
  const auto entry = anime::db.entry(item.episode.animeId());

  if (!entry) return false;

  const auto range = item.episode.episodeNumberRange();

  return range && range->second > entry->watched_episodes;
}

TorrentCategory torrentCategory(const FeedItem& item) {
  if (QString::fromStdString(item.category.value).contains(u"Batch"_s, Qt::CaseInsensitive)) {
    return TorrentCategory::Batch;
  }

  if (isBatchRelease(item.episode)) return TorrentCategory::Batch;

  // v1 checks the anime type element against Anitomy's keywords here. v2 has no such check, so the
  // closest equivalent is used: openings, endings and previews are not regular episodes.
  if (!recognition::isValidEpisodeType(item.episode)) return TorrentCategory::Other;

  if (const auto range = item.episode.episodeNumberRange();
      range && range->first != range->second) {
    return TorrentCategory::Batch;
  }

  if (item.episode.contains(anitomy::ElementKind::FileExtension)) {
    if (!recognition::isVideoFile(item.episode)) return TorrentCategory::Other;
  }

  return TorrentCategory::Anime;
}

}  // namespace

bool FeedItem::isDiscarded() const {
  switch (state) {
    case FeedItemState::DiscardedNormal:
    case FeedItemState::DiscardedInactive:
    case FeedItemState::DiscardedHidden:
      return true;
    default:
      return false;
  }
}

QDateTime parseDate(const FeedItem& item) {
  auto value = QString::fromStdString(item.pub_date).trimmed();

  // Qt only accepts a numeric zone offset, while RFC 2822 also allows the obsolete zone names that
  // some providers still use (e.g. Tokyo Toshokan publishes dates ending in `GMT`).
  static const std::map<QString, QString> zones{
      {u"UT"_s, u"+0000"_s},  {u"GMT"_s, u"+0000"_s}, {u"UTC"_s, u"+0000"_s},
      {u"EST"_s, u"-0500"_s}, {u"EDT"_s, u"-0400"_s}, {u"CST"_s, u"-0600"_s},
      {u"CDT"_s, u"-0500"_s}, {u"MST"_s, u"-0700"_s}, {u"MDT"_s, u"-0600"_s},
      {u"PST"_s, u"-0800"_s}, {u"PDT"_s, u"-0700"_s},
  };

  if (const auto pos = value.lastIndexOf(u' '); pos > 0) {
    if (const auto it = zones.find(value.mid(pos + 1).toUpper()); it != zones.end()) {
      value.replace(pos + 1, value.size() - pos - 1, it->second);
    }
  }

  auto date = QDateTime::fromString(value, Qt::RFC2822Date);
  if (!date.isValid()) date = QDateTime::fromString(value, Qt::ISODate);
  return date;
}

quint64 parseSizeString(QString value) {
  static const std::map<QString, quint64> units{
      {u"KB"_s, 1000ULL},
      {u"KiB"_s, 1024ULL},
      {u"MB"_s, 1000ULL * 1000},
      {u"MiB"_s, 1024ULL * 1024},
      {u"GB"_s, 1000ULL * 1000 * 1000},
      {u"GiB"_s, 1024ULL * 1024 * 1024},
      {u"TB"_s, 1000ULL * 1000 * 1000 * 1000},
      {u"TiB"_s, 1024ULL * 1024 * 1024 * 1024},
  };

  quint64 unit = 1;

  if (const auto pos = value.indexOf(QRegularExpression{u"[^0-9.]"_s}); pos >= 0) {
    const auto name = value.mid(pos).trimmed();
    value.truncate(pos);

    for (const auto& [key, value] : units) {
      if (compareStrings(name.toStdString(), key.toStdString(), Qt::CaseInsensitive) == 0) {
        unit = value;
        break;
      }
    }
  }

  return static_cast<quint64>(unit * value.toDouble());
}

FeedSource feedSource(const std::string& channelLink) {
  static const std::map<QString, FeedSource> sources{
      {u"acgnx"_s, FeedSource::Acgnx},
      {u"anidex"_s, FeedSource::AniDex},
      {u"animebytes"_s, FeedSource::AnimeBytes},
      {u"animetosho"_s, FeedSource::AnimeTosho},
      {u"minglong"_s, FeedSource::Minglong},
      {u"nyaa.net"_s, FeedSource::NyaaPantsu},
      {u"nyaa.pantsu"_s, FeedSource::NyaaPantsu},
      {u"nyaa.pt"_s, FeedSource::NyaaPantsu},
      {u"nyaa.si"_s, FeedSource::NyaaSi},
      {u"subsplease"_s, FeedSource::SubsPlease},
      {u"tokyotosho"_s, FeedSource::TokyoToshokan},
  };

  const auto host = QUrl{QString::fromStdString(channelLink)}.host();

  for (const auto& [name, source] : sources) {
    if (host.contains(name, Qt::CaseInsensitive)) return source;
  }

  return FeedSource::Unknown;
}

std::optional<Feed> parseFeed(const QString& data) {
  const auto parsed = rss::parse(data);

  if (!parsed) return std::nullopt;

  Feed feed;
  feed.channel = parsed->channel;
  feed.source = feedSource(feed.channel.link);

  for (const auto& item : parsed->items) {
    FeedItem feedItem{item};
    parseItemFromSource(feed.source, feedItem);
    feed.items.push_back(std::move(feedItem));
  }

  return feed;
}

void examineFeed(Feed& feed) {
  for (auto& item : feed.items) {
    item.episode = recognition::parse(item.title);
    item.episode.setAnimeId(recognition::identify(item.episode));
    item.torrent_category = torrentCategory(item);
    item.new_episode = isNewEpisode(item);
  }
}

QString torrentCategoryName(const TorrentCategory category) {
  switch (category) {
    case TorrentCategory::Batch:
      return QCoreApplication::translate("track", "Batch");
    case TorrentCategory::Other:
      return QCoreApplication::translate("track", "Other");
    default:
      return QCoreApplication::translate("track", "Anime");
  }
}

}  // namespace track
