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

#include "track/media_candidates.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using anisthesia::Media;
using anisthesia::MediaInfo;
using anisthesia::MediaInfoType;
using anisthesia::MediaState;
using anisthesia::Result;

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

Media session(MediaState state, std::string title, std::string url = {}) {
  Media media{.state = state};
  media.information.push_back({MediaInfoType::Title, std::move(title)});
  if (!url.empty()) media.information.push_back({MediaInfoType::Url, std::move(url)});
  return media;
}

Media piece(MediaInfoType type, std::string value) {
  Media media;
  media.information.push_back({type, std::move(value)});
  return media;
}

std::string value(const Media& media, MediaInfoType type) {
  for (const auto& information : media.information) {
    if (information.type == type) return information.value;
  }
  return {};
}

Result browser(std::int64_t process, std::vector<Media> media) {
  Result result;
  result.player.type = anisthesia::PlayerType::WebBrowser;
  result.id.process = process;
  result.media = std::move(media);
  return result;
}

}  // namespace

int main() {
  using namespace track::media;

  // One process with two MPRIS sessions, as Firefox reports two tabs.
  const auto tabs = browser(1, {session(MediaState::Playing, "Frieren - 05", "https://a.test/5"),
                                session(MediaState::Playing, "Lecture", "https://b.test/x")});
  const auto split = splitMedia(tabs);
  require(split.size() == 2, "each MPRIS session should be a candidate of its own");
  require(value(split[0], MediaInfoType::Title) == "Frieren - 05" &&
              value(split[0], MediaInfoType::Url) == "https://a.test/5",
          "a session should keep its own title and address");
  require(value(split[1], MediaInfoType::Url) == "https://b.test/x",
          "the second session should not lend its address to the first");

  auto timed = session(MediaState::Paused, "Frieren - 06");
  timed.position = std::chrono::seconds{30};
  timed.duration = std::chrono::minutes{24};
  const auto kept = splitMedia(browser(1, {timed}));
  require(kept.size() == 1 && kept[0].state == MediaState::Paused &&
              kept[0].position == std::chrono::seconds{30} &&
              kept[0].duration == std::chrono::minutes{24},
          "state, position and duration should survive");

  // Windows reports window title and UI Automation values as single pieces of the same media.
  const auto windows = browser(
      2, {piece(MediaInfoType::Unknown, "Frieren - 05 - Firefox"),
          piece(MediaInfoType::Url, "https://a.test/5"),
          piece(MediaInfoType::Title, "Frieren - 05"), piece(MediaInfoType::Tab, "Frieren - 05")});
  const auto merged = splitMedia(windows);
  require(merged.size() == 1 && merged[0].information.size() == 4,
          "single pieces of information should be merged into one candidate");

  // A media session without a known state is still a session.
  const auto unknown =
      splitMedia(browser(3, {session(MediaState::Unknown, "A", "https://a.test"),
                             session(MediaState::Unknown, "B", "https://b.test")}));
  require(unknown.size() == 2, "sessions with several pieces should not be merged");

  require(splitMedia(browser(4, {})).empty(), "no media should give no candidates");

  // Sessions and single pieces in one result stay apart, the pieces merged where they first appear.
  const auto mixedPieces =
      splitMedia(browser(9, {piece(MediaInfoType::File, "/a/Frieren - 05.mkv"),
                             session(MediaState::Playing, "Lecture", "https://b.test/x"),
                             piece(MediaInfoType::Unknown, "Frieren - 05 - mpv")}));
  require(mixedPieces.size() == 2 && mixedPieces[0].information.size() == 2 &&
              value(mixedPieces[0], MediaInfoType::File) == "/a/Frieren - 05.mkv" &&
              value(mixedPieces[1], MediaInfoType::Url) == "https://b.test/x",
          "a session should not take in single pieces of the same result");

  // A paused session comes after a playing one, even in the player followed so far.
  const std::vector<Result> mixed{
      browser(5, {session(MediaState::Paused, "Paused - 01", "https://p.test")}),
      browser(6, {session(MediaState::Playing, "Playing - 02", "https://q.test")})};
  auto ordered = orderCandidates(mixed, {.process = 5}, std::nullopt);
  require(ordered.size() == 2 && value(ordered[0].media, MediaInfoType::Title) == "Playing - 02",
          "a paused session should come last");
  require(ordered[0].result == &mixed[1], "a candidate should point at its own result");

  // The player followed so far keeps its place while playing.
  const std::vector<Result> both{
      browser(7, {session(MediaState::Playing, "Other - 03", "https://o.test")}),
      browser(8, {session(MediaState::Playing, "Current - 04", "https://c.test")})};
  ordered = orderCandidates(both, {.process = 8}, std::nullopt);
  require(value(ordered[0].media, MediaInfoType::Title) == "Current - 04",
          "the current player should come first");

  // Within the same process, the session followed so far comes first.
  const std::vector<Result> tabList{tabs};
  const auto current = session(MediaState::Playing, "Lecture", "https://b.test/x");
  ordered = orderCandidates(tabList, {.process = 1}, current);
  require(value(ordered[0].media, MediaInfoType::Title) == "Lecture",
          "the current session should come first within its player");
  ordered = orderCandidates(tabList, {.process = 2}, current);
  require(value(ordered[0].media, MediaInfoType::Title) == "Frieren - 05",
          "another player's session should not be taken for the current one");

  return EXIT_SUCCESS;
}
