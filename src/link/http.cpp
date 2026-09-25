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

#include "http.hpp"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "akyuu/network.hpp"
#include "akyuu/script.hpp"
#include "akyuu/settings.hpp"
#include "base/log.hpp"

namespace link::http {

using namespace Qt::StringLiterals;

void announce(const track::Episode& episode, const bool force) {
  if (!force && !akyuu::settings.httpShareEnabled()) return;

  const auto url = QString::fromStdString(akyuu::settings.httpShareUrl());

  if (url.isEmpty()) return;

  // The body goes into a request, so the values are percent-encoded.
  const auto body = akyuu::replaceVariables(
      QString::fromStdString(akyuu::settings.httpShareFormat()), episode, true);

  QNetworkRequest request{QUrl{url}};
  request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);

  const auto reply = akyuu::network()->post(request, body.toUtf8());

  QObject::connect(reply, &QNetworkReply::finished, reply, [reply]() {
    if (reply->error() != QNetworkReply::NoError) {
      qWarning() << "Could not announce over HTTP:" << reply->errorString();
    }
    reply->deleteLater();
  });
}

}  // namespace link::http
