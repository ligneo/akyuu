/*
** Taiga
** Copyright (C) 2010-2021, Eren Okka
** Copyright (C) 2026, cenky <cenkkgl@gmail.com>
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "taiga/announce.h"

#include "base/log.h"
#include "base/string.h"
#include "track/episode.h"
#include "link/discord.h"
#include "link/http.h"
#include "link/mirc.h"
#include "media/anime_db.h"
#include "media/anime_util.h"
#include "sync/anilist_util.h"
#include "sync/kitsu_util.h"
#include "sync/myanimelist_util.h"
#include "sync/service.h"
#include "taiga/script.h"
#include "taiga/settings.h"
#include "ui/ui.h"

namespace taiga {

Announcer::~Announcer() {
  share::discord::Shutdown();
}

void Announcer::Clear(int modes, bool force) {
  if (modes & kAnnounceToDiscord)
    if (settings.GetShareDiscordEnabled() || force)
      share::discord::ClearPresence();

  if (modes & kAnnounceToHttp)
    if (settings.GetShareHttpEnabled() || force)
      share::http::Send(WstrToStr(settings.GetShareHttpUrl()), {});
}

void Announcer::Do(int modes, anime::Episode* episode, bool force) {
  if (!force && !settings.GetAppOptionEnableSharing())
    return;

  if (!episode)
    episode = &CurrentEpisode;

  const auto anime_item = anime::db.Find(episode->anime_id);

  if (anime_item) {
    if (!force && anime_item->GetMyPrivate()) {
      return;  // Avoid sharing private anime
    }
  }

  if (modes & kAnnounceToHttp) {
    if (settings.GetShareHttpEnabled() || force) {
      LOGD(L"HTTP");
      const auto data = WstrToStr(ReplaceVariables(
          settings.GetShareHttpFormat(), *episode, true, force));
      share::http::Send(WstrToStr(settings.GetShareHttpUrl()), data);
    }
  }

  if (!anime::IsValidId(episode->anime_id))
    return;

  if (modes & kAnnounceToDiscord) {
    if (settings.GetShareDiscordEnabled() || force) {
      LOGD(L"Discord");

      std::wstring details = L"%title%";
      details = ReplaceVariables(details, *episode, false, force);
      details = LimitText(details, 64);

      std::wstring state =
          L"$if(%episode%,Episode %episode%$if(%total%,/%total%) )";
      if (settings.GetShareDiscordGroupEnabled())
        state += L"$if(%group%,by %group%)";
      state = ReplaceVariables(state, *episode, false, force);
      state = LimitText(state, 64);

      std::wstring large_image;
      if (anime_item)
        large_image = anime_item->GetImageUrl();

      std::wstring button_url;
      if (anime_item) {
        switch (services::GetCurrentServiceId()) {
          case services::ServiceId::MyAnimeList:
            button_url = services::myanimelist::GetAnimePage(*anime_item);
            break;
          case services::ServiceId::Kitsu:
            button_url = services::kitsu::GetAnimePage(*anime_item);
            break;
          case services::ServiceId::AniList:
            button_url = services::anilist::GetAnimePage(*anime_item);
            break;
        }
      }

      auto timestamp = std::time(nullptr);
      if (!force)
        timestamp -= settings.GetSyncUpdateDelay();

      share::discord::UpdatePresence(WstrToStr(details), WstrToStr(state),
                                    WstrToStr(large_image), "View Anime",
                                    WstrToStr(button_url), timestamp);
    }
  }

  if (modes & kAnnounceToMirc) {
    if (settings.GetShareMircEnabled() || force) {
      LOGD(L"mIRC");
      const auto data = ReplaceVariables(settings.GetShareMircFormat(),
                                         *episode, false, force);
      if (!share::mirc::Send(settings.GetShareMircService(),
                            settings.GetShareMircChannels(),
                            data, settings.GetShareMircMode(),
                            settings.GetShareMircUseMeAction(),
                            settings.GetShareMircMultiServer())) {
        ui::OnMircDdeConnectionFail();
      }
    }
  }
}

}  // namespace taiga
