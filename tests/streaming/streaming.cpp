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

#include <anitomy.hpp>
#include <cstdlib>
#include <iostream>

#include "akyuu/settings.hpp"
#include "track/media_stream.hpp"

namespace {

std::vector<std::string> disabledProviders;

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

void requireEpisode(const std::string& title, const std::string& anime, const std::string& number) {
  std::string parsedTitle;
  std::string parsedNumber;
  for (const auto& element : anitomy::parse(title)) {
    if (element.kind == anitomy::ElementKind::Title) parsedTitle = element.value;
    if (element.kind == anitomy::ElementKind::Episode) parsedNumber = element.value;
  }
  require(parsedTitle == anime, "browser title should preserve the anime name");
  require(parsedNumber == number, "browser title should preserve the episode number");
}

}  // namespace

// Use an in-memory provider preference so these tests never open account or settings files.
namespace akyuu {
QString Settings::fileName() const {
  return {};
}
std::vector<std::string> Settings::disabledStreamingProviders() const {
  return disabledProviders;
}
}  // namespace akyuu

int main() {
  using namespace track::recognition;
  const std::string url = "https://www.youtube.com/watch?v=test";
  const std::string raw = "Clannad After Story Anime Full Episode 23 (English Dub) ";

  for (const auto& title : {raw, raw + "- YouTube"}) {
    const auto extracted = titleFromBrowserMedia(url, title);
    require(extracted.has_value(), "YouTube should accept page titles and bare MPRIS titles");
    requireEpisode(*extracted, "Clannad After Story", "23");
  }
  const auto withoutUrl = titleFromBrowserMedia({}, raw);
  require(withoutUrl.has_value(), "a bare browser media title should reach recognition");
  requireEpisode(*withoutUrl, "Clannad After Story", "23");
  require(titleFromBrowserMedia("www.youtube.com/watch?v=test", raw) == withoutUrl,
          "Windows address bar values may omit the scheme");
  require(!titleFromBrowserMedia({}, "   "), "an empty browser title must not be accepted");
  requireEpisode(cleanBrowserTitle("Clannad After Story Full Episode 23"), "Clannad After Story",
                 "23");
  requireEpisode(cleanBrowserTitle("Full Metal Panic! Episode 2"), "Full Metal Panic!", "2");
  require(cleanBrowserTitle("Full Metal Alchemist") == "Full Metal Alchemist",
          "words inside an anime title must not be removed");
  require(cleanBrowserTitle("Anime Full Episode") == "Anime Full Episode",
          "a label without a numbered episode must not be rewritten");
  require(cleanBrowserTitle("Anime Full Metal Episode 2") == "Anime Full Metal Episode 2",
          "unrelated words must not be rewritten");
  require(cleanBrowserTitle("Clannad Full Episode 7.5") == "Clannad - 7.5",
          "fractional episode numbers must be retained");
  require(cleanBrowserTitle("Clannad Full Episode 1-3") == "Clannad - 1-3",
          "episode ranges must be retained");
  require(!titleFromStreamingProvider(url, "YouTube"), "a generic page title is not an episode");
  require(!titleFromStreamingProvider("https://www.youtube.com/", raw),
          "a provider home page is not a video page");
  require(!titleFromStreamingProvider("https://example.com/watch?v=test", raw),
          "an unknown provider must not be accepted");
  require(!titleFromStreamingProvider("https://example.com/?url=youtube.com/watch", raw),
          "a provider name in a query must not be accepted");
  disabledProviders = {"YouTube"};
  require(!titleFromBrowserMedia({}, raw),
          "missing URLs must not bypass disabled provider preferences");
  require(!titleFromBrowserMedia(url, raw), "browser media must honor disabled providers");
  require(!titleFromStreamingProvider(url, raw), "a disabled provider must not be accepted");
  std::cout << "Streaming title checks passed\n";
}
