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

#include "media_stream.hpp"

#include <algorithm>

#include "akyuu/settings.hpp"
#include "base/string.hpp"

namespace track::recognition {

namespace {

QString applyTitlePattern(const StreamData& stream, const QString& title) {
  const auto match = stream.titlePattern.match(title, 0, QRegularExpression::NormalMatch,
                                               QRegularExpression::AnchoredMatchOption);
  if (!match.hasMatch()) return {};

  // Use the first non-empty capture. Patterns also match pages without a video (e.g. the home page
  // of the provider), in which case there is nothing to capture and the title is discarded.
  for (int i = 1; i <= match.lastCapturedIndex(); ++i) {
    if (!match.captured(i).isEmpty()) return match.captured(i).trimmed();
  }

  return {};
}

QString cleanStreamTitle(const StreamData& stream, QString title) {
  switch (stream.id) {
    case Stream::Adn:
      title.replace(" : ", " - ");
      break;

    case Stream::Ann:
      title.remove(QRegularExpression{u" \\((?:s|d)(?:, uncut)?\\)"_s});
      break;

    case Stream::Plex:
      title.remove(u" · "_s);
      break;

    case Stream::RokuChannel:
    case Stream::Tubi:
      title.replace(QRegularExpression{u" S(\\d+):E(\\d+) "_s}, u" S\\1E\\2 "_s);
      break;

    case Stream::Vrv:
      title.replace(": EP ", " - EP ");
      break;

    case Stream::Wakanim: {
      static const QRegularExpression pattern{
          u"(?:Episode (\\d+)|Film|Movie) - (?:ENGDUB - )?(.+)"_s};
      if (const auto match = pattern.match(title); match.hasMatch()) {
        title = match.captured(2);
        if (!match.captured(1).isEmpty()) title += u" - %1"_s.arg(match.captured(1));
      }
      break;
    }

    default:
      break;
  }

  return title.trimmed();
}

}  // namespace

const std::vector<StreamData>& streamData() {
  // clang-format off
  static const std::vector<StreamData> data{
    {
      Stream::Adn,
      u"Anime Digital Network"_s,
      u"https://animationdigitalnetwork.fr/"_s,
      QRegularExpression{u"anima(?:tion|ted)digitalnetwork\\.(?:fr|com)/video/"_s},
      QRegularExpression{u"(.+) - streaming -.* ADN"_s},
    },
    {
      Stream::Animelab,
      u"AnimeLab"_s,
      u"https://www.animelab.com"_s,
      QRegularExpression{u"animelab\\.com/player/"_s},
      QRegularExpression{u"AnimeLab - (.+)"_s},
    },
    {
      Stream::Ann,
      u"Anime News Network"_s,
      u"https://www.animenewsnetwork.com/video/"_s,
      QRegularExpression{u"animenewsnetwork\\.(?:com|cc)/video/[0-9]+"_s},
      QRegularExpression{u"(.+) - Anime News Network"_s},
    },
    {
      Stream::Bilibili,
      u"Bilibili"_s,
      u"https://www.bilibili.tv/en/anime"_s,
      QRegularExpression{u"bilibili\\.tv/[^/]+/play/[0-9]+"_s},
      QRegularExpression{u"(.+) - Bilibili"_s},
    },
    {
      Stream::Crunchyroll,
      u"Crunchyroll"_s,
      u"https://www.crunchyroll.com"_s,
      QRegularExpression{u"crunchyroll\\.com/(?:[a-z]{2}(?:-[a-z]{2})?/)?watch/"_s},
      QRegularExpression{u"(?:Watch )?(.+?)(?: - Watch on Crunchyroll| - Crunchyroll)"_s},
    },
    {
      Stream::Jellyfin,
      u"Jellyfin Web App"_s,
      u"https://jellyfin.org"_s,
      QRegularExpression{u"^.+/web/(?:index\\.html)?#!/video"_s},
      QRegularExpression{u"Jellyfin|(.+)"_s},
    },
    {
      Stream::Plex,
      u"Plex Web App"_s,
      u"https://www.plex.tv"_s,
      QRegularExpression{
          u"^app\\.plex\\.tv/desktop|"
          "^[^/]*?plex\\.tv/web/|"
          "^localhost:32400/web/|"
          "^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}:32400/web/|"
          "^plex\\.[a-z0-9-]+\\.[a-z0-9-]+|"
          "^[^/]*[a-z0-9-]+\\.[a-z0-9-]+/plex"_s},
      QRegularExpression{u"Plex|(?:▶ )?(.+)"_s},
    },
    {
      Stream::RokuChannel,
      u"Roku Channel"_s,
      u"https://therokuchannel.roku.com"_s,
      QRegularExpression{u"therokuchannel\\.roku\\.com/watch/.+"_s},
      QRegularExpression{u"Watch (.+) Online for Free \\| The Roku Channel \\| Roku"_s},
    },
    {
      Stream::Tubi,
      u"Tubi"_s,
      u"https://tubitv.com"_s,
      QRegularExpression{u"tubitv\\.com/tv-shows/.+"_s},
      QRegularExpression{u"Watch (.+) - Free TV Shows \\| Tubi"_s},
    },
    {
      Stream::Veoh,
      u"Veoh"_s,
      u"http://www.veoh.com"_s,
      QRegularExpression{u"veoh\\.com/watch/"_s},
      QRegularExpression{u"Watch Videos Online \\| (.+) \\| Veoh\\.com"_s},
    },
    {
      Stream::Viz,
      u"VIZ"_s,
      u"https://www.viz.com/watch"_s,
      QRegularExpression{u"viz\\.com/watch/streaming/[^/]+-(?:episode-[0-9]+|movie)/"_s},
      QRegularExpression{u"(.+) // VIZ"_s},
    },
    {
      Stream::Vrv,
      u"VRV"_s,
      u"https://vrv.co"_s,
      QRegularExpression{u"vrv\\.co/watch/"_s},
      QRegularExpression{u"(.+) - Watch on VRV"_s},
    },
    {
      Stream::Wakanim,
      u"Wakanim"_s,
      u"https://www.wakanim.tv"_s,
      QRegularExpression{u"wakanim\\.tv/[^/]+/v2/catalogue/episode/[^/]+/"_s},
      QRegularExpression{u"(.+) (?:auf|on|sur) Wakanim\\.TV.*"_s},
    },
    {
      Stream::Yahoo,
      u"Yahoo View"_s,
      u"https://view.yahoo.com"_s,
      QRegularExpression{u"view\\.yahoo\\.com/show/[^/]+/episode/[^/]+/"_s},
      QRegularExpression{u"Watch .+ Free Online - (.+) \\| Yahoo View"_s},
    },
    {
      Stream::Youtube,
      u"YouTube"_s,
      u"https://www.youtube.com"_s,
      QRegularExpression{u"youtube\\.com/watch"_s},
      QRegularExpression{u"YouTube|(?:▶ )?(.+) - YouTube"_s},
    },
  };
  // clang-format on

  return data;
}

bool isStreamEnabled(const StreamData& stream) {
  const auto disabled = akyuu::settings.disabledStreamingProviders();
  return !std::ranges::contains(disabled, stream.name.toStdString());
}

std::optional<std::string> titleFromStreamingProvider(const std::string& url,
                                                      const std::string& title) {
  const auto value = QString::fromStdString(url);

  const auto it = std::ranges::find_if(streamData(), [&value](const StreamData& stream) {
    return stream.urlPattern.match(value).hasMatch();
  });

  if (it == streamData().end()) return std::nullopt;

  if (!isStreamEnabled(*it)) return std::nullopt;

  const auto cleaned = cleanStreamTitle(*it, applyTitlePattern(*it, QString::fromStdString(title)));

  if (cleaned.isEmpty()) return std::nullopt;

  return cleaned.toStdString();
}

}  // namespace track::recognition
