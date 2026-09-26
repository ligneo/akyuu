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

#include <QSet>
#include <functional>

#include "sync/service.hpp"

class QRestReply;

namespace sync::myanimelist {

// Akyuu's own client (myanimelist.net/apiconfig), of the "other" type: it has no secret, and the
// code is exchanged with PKCE instead. The redirect URL is a static page that only shows the code
// for the user to paste; the code never leaves the browser.
constexpr auto kClientId = "531c14b640e8523a0cafdb3f00ae2368";
constexpr auto kRedirectUrl = "https://cenky.dev/akyuu/mal/";
constexpr auto kApiUrl = "https://api.myanimelist.net/v2";
constexpr auto kTokenUrl = "https://myanimelist.net/v1/oauth2/token";

class Service final : public sync::Service {
public:
  Service();
  ~Service() = default;

  static Service* instance();

  void authenticateUser();
  void requestAccessToken(const QString& authorizationCode, const QString& codeVerifier);
  void fetchAnime(const int id);
  void search(const SearchParams& params, const int offset = 0);
  void fetchListEntries(const int offset = 0, QSet<int> fetchedIds = {});
  void addListEntry(const int id, const anime::list::Fields dirty);
  void deleteListEntry(const int id);
  void updateListEntry(const int id, const anime::list::Fields dirty);

private:
  void refreshAccessToken(std::function<void()> onSuccess);
  bool retryOnTokenExpiry(QRestReply& reply, std::function<void()> retry);
};

}  // namespace sync::myanimelist
