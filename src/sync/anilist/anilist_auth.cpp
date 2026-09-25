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

#include <QJsonDocument>
#include <QJsonObject>
#include <QRestReply>

#include "akyuu/accounts.hpp"
#include "anilist.hpp"
#include "sync/anilist/anilist_error.hpp"
#include "sync/anilist/anilist_utils.hpp"

namespace sync::anilist {

void Service::setAccessToken(const QString& token) {
  akyuu::accounts.setAnilistToken(token.toStdString());
  api_.setBearerToken(token.toUtf8());
}

void Service::authenticateUser() {
  const QJsonDocument data{QJsonObject{
      {"query", gql("Viewer")},
  }};

  const auto callback = [this](QRestReply& reply) {
    if (isError(reply)) {
      handleError(*this, reply);
      emit authenticationCompleted(false);
      return;
    }

    const auto viewer = reply.readJson().and_then([](const QJsonDocument& json) {
      return std::make_optional(json["data"]["Viewer"].toObject());
    });

    if (!viewer) {
      handleError(*this, reply, "Could not parse user object.");
      emit authenticationCompleted(false);
      return;
    }

    akyuu::accounts.setAnilistUsername((*viewer)["name"].toString().toStdString());
    akyuu::accounts.setAnilistRatingSystem(
        (*viewer)["mediaListOptions"]["scoreFormat"].toString().toStdString());

    emit authenticationCompleted(true);
  };

  manager_.post(api_.createRequest(), data, this, callback);
}

}  // namespace sync::anilist
