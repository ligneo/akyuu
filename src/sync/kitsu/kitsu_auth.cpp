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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRestReply>
#include <QUrlQuery>
#include <QtLogging>

#include "akyuu/accounts.hpp"
#include "base/form.hpp"
#include "base/string.hpp"
#include "kitsu.hpp"
#include "sync/kitsu/kitsu_error.hpp"
#include "sync/kitsu/kitsu_utils.hpp"

namespace services::kitsu {

namespace {

// The user signed in, or the one whose library is read without signing in
QUrlQuery userQuery(const bool authenticated) {
  QUrlQuery query =
      authenticated
          ? QUrlQuery{{u"filter[self]"_s, u"true"_s}}
          : QUrlQuery{{u"filter[slug]"_s, QString::fromStdString(akyuu::accounts.kitsuUsername())}};
  query.addQueryItem(u"fields[users]"_s, userFields());
  return query;
}

void applyUser(const QJsonObject& user) {
  const auto attributes = user["attributes"].toObject();
  akyuu::accounts.setKitsuUserId(user["id"].toString().toStdString());
  akyuu::accounts.setKitsuDisplayName(attributes["name"].toString().toStdString());
  akyuu::accounts.setKitsuUsername(attributes["slug"].toString().toStdString());
  // For the profile page, which asks for nothing itself
  akyuu::accounts.setServiceAvatar("kitsu", attributes["avatar"]["large"].toString().toStdString());
  akyuu::accounts.setServiceBanner("kitsu",
                                   attributes["coverImage"]["large"].toString().toStdString());
}

}  // namespace

bool Service::retryOnTokenExpiry(QRestReply& reply, std::function<void()> retry) {
  if (!isTokenExpired(reply)) return false;
  refreshAccessToken(std::move(retry));
  return true;
}

void Service::authenticateUser() {
  QNetworkRequest request{QUrl{kTokenUrl}};
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

  auto username = akyuu::accounts.kitsuEmail();
  if (username.empty()) username = akyuu::accounts.kitsuUsername();

  // Resource Owner Password Credentials Grant
  // https://tools.ietf.org/html/rfc6749#section-4.3
  const base::FormFields body{{
      {"grant_type", "password"},
      {"username", QString::fromStdString(username)},
      {"password", QString::fromStdString(akyuu::accounts.kitsuPassword())},
      {"client_id", kClientId},
      {"client_secret", kClientSecret},
  }};

  const auto callback = [this](QRestReply& reply) {
    if (isError(reply)) {
      handleError(*this, reply);
      emit authenticationCompleted(false);
      return;
    }

    const auto json = reply.readJson();
    if (!json) {
      handleError(*this, reply, "Could not parse authentication data.");
      emit authenticationCompleted(false);
      return;
    }

    const auto root = json->object();
    const auto accessToken = root["access_token"].toString();
    akyuu::accounts.setKitsuAccessToken(accessToken.toStdString());
    akyuu::accounts.setKitsuRefreshToken(root["refresh_token"].toString().toStdString());
    api_.setBearerToken(accessToken.toUtf8());

    // Kitsu's token response doesn't include user information, so we need to make an additional
    // request to resolve it.
    resolveUser();
  };

  manager_.post(request, base::formUrlEncode(body), this, callback);
}

void Service::refreshAccessToken(std::function<void()> onSuccess) {
  const auto refreshToken = akyuu::accounts.kitsuRefreshToken();

  if (refreshToken.empty()) {
    emit errorOccurred("Refresh token is unavailable.");
    emit authenticationCompleted(false);
    return;
  }

  QNetworkRequest request{QUrl{kTokenUrl}};
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

  const base::FormFields body{{
      {"grant_type", "refresh_token"},
      {"refresh_token", QString::fromStdString(refreshToken)},
      {"client_id", kClientId},
      {"client_secret", kClientSecret},
  }};

  const auto callback = [this, onSuccess](QRestReply& reply) {
    if (isError(reply)) {
      handleError(*this, reply);
      emit authenticationCompleted(false);
      return;
    }

    const auto json = reply.readJson();
    if (!json) {
      handleError(*this, reply, "Could not parse authentication data.");
      emit authenticationCompleted(false);
      return;
    }

    const auto root = json->object();
    const auto accessToken = root["access_token"].toString();
    akyuu::accounts.setKitsuAccessToken(accessToken.toStdString());
    akyuu::accounts.setKitsuRefreshToken(root["refresh_token"].toString().toStdString());
    api_.setBearerToken(accessToken.toUtf8());

    if (onSuccess) onSuccess();
  };

  manager_.post(request, base::formUrlEncode(body), this, callback);
}

////////////////////////////////////////////////////////////////////////////////

void Service::resolveUser(std::function<void()> onSuccess) {
  const bool authenticated = !akyuu::accounts.kitsuAccessToken().empty();
  const auto query = userQuery(authenticated);

  const auto callback = [this, authenticated, onSuccess](QRestReply& reply) {
    if (isError(reply)) {
      if (retryOnTokenExpiry(reply, [this, onSuccess] { resolveUser(onSuccess); })) return;
      handleError(*this, reply);
      if (authenticated) emit authenticationCompleted(false);
      return;
    }

    const auto json = reply.readJson();
    const auto data = json ? json->object()["data"].toArray() : QJsonArray{};

    if (data.isEmpty()) {
      handleError(*this, reply, "Could not parse user object.");
      if (authenticated) emit authenticationCompleted(false);
      return;
    }

    const auto user = data.first().toObject();
    const auto attributes = user["attributes"].toObject();

    applyUser(user);

    if (authenticated) {
      akyuu::accounts.setKitsuEmail(attributes["email"].toString().toStdString());
      akyuu::accounts.setKitsuRatingSystem(attributes["ratingSystem"].toString().toStdString());
      emit authenticationCompleted(true);
    }

    if (onSuccess) onSuccess();
  };

  manager_.get(api_.createRequest(u"/users"_s, query), this, callback);
}

void Service::refreshUser() {
  const bool authenticated = !akyuu::accounts.kitsuAccessToken().empty();
  if (!authenticated && akyuu::accounts.kitsuUsername().empty()) return;

  manager_.get(api_.createRequest(u"/users"_s, userQuery(authenticated)), this,
               [this](QRestReply& reply) {
                 if (isError(reply)) {
                   qWarning("Could not refresh the user: %s", qPrintable(reply.errorString()));
                   return;
                 }
                 const auto json = reply.readJson();
                 const auto data = json ? json->object()["data"].toArray() : QJsonArray{};
                 if (data.isEmpty()) return;
                 applyUser(data.first().toObject());
                 emit userRefreshed();
               });
}

}  // namespace services::kitsu
