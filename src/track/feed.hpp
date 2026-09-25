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

#pragma once

#include <QDateTime>
#include <QString>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/rss.hpp"
#include "track/episode.hpp"

namespace track {

enum class FeedItemState {
  Blank,
  DiscardedNormal,
  DiscardedInactive,
  DiscardedHidden,
  Selected,
};

enum class TorrentCategory {
  Anime,
  Batch,
  Other,
};

enum class FeedSource {
  Unknown,
  Acgnx,
  AniDex,
  AnimeBytes,
  AnimeTosho,
  Minglong,
  NyaaPantsu,
  NyaaSi,
  SubsPlease,
  TokyoToshokan,
};

// Providers publish the same information in different places, so the values below are filled in
// separately for each one.
struct FeedItem : rss::Item {
  FeedItem() = default;
  explicit FeedItem(const rss::Item& item) : rss::Item{item} {}

  // Filled in by `examineFeed()`, so that the title is only parsed once.
  Episode episode;
  TorrentCategory torrent_category = TorrentCategory::Anime;
  FeedItemState state = FeedItemState::Blank;
  // True when the item is ahead of what has been watched, as in v1's `MarkNewEpisodes()`
  bool new_episode = false;

  std::string info_link;
  std::string magnet_link;
  std::optional<int> seeders;
  std::optional<int> leechers;
  std::optional<int> downloads;
  quint64 file_size = 0;

  bool isDiscarded() const;
};

struct Feed {
  FeedSource source = FeedSource::Unknown;
  rss::Channel channel;
  std::vector<FeedItem> items;
};

FeedSource feedSource(const std::string& channelLink);
quint64 parseSizeString(QString value);
QDateTime parseDate(const FeedItem& item);
std::optional<Feed> parseFeed(const QString& data);

// Parses and identifies every item, then categorizes it. Runs once per feed, as in v1.
void examineFeed(Feed& feed);

QString torrentCategoryName(const TorrentCategory category);

}  // namespace track
