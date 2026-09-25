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

#include "network.hpp"

#include <QNetworkProxy>
#include <QNetworkReply>
#include <QRestReply>

#include "base/log.hpp"
#include "base/string.hpp"
#include "akyuu/application.hpp"
#include "akyuu/config.h"
#include "akyuu/settings.hpp"

namespace taiga {

namespace {

QNetworkProxy buildProxy() {
  const auto host = QString::fromStdString(settings.proxyHost());
  if (host.isEmpty()) return QNetworkProxy{};

  QNetworkProxy proxy;
  proxy.setType(settings.proxyType());
  proxy.setHostName(host);
  if (const auto port = settings.proxyPort(); port >= 0) {
    proxy.setPort(static_cast<quint16>(port));
  }

  if (const auto username = settings.proxyUsername(); !username.empty()) {
    proxy.setUser(QString::fromStdString(username));
  }
  if (const auto password = settings.proxyPassword(); !password.empty()) {
    proxy.setPassword(QString::fromStdString(password));
  }

  return proxy;
}

}  // namespace

NetworkAccessManager::NetworkAccessManager(QObject* parent) : QNetworkAccessManager{parent} {
  setAutoDeleteReplies(true);
  setTransferTimeout(std::chrono::seconds{10});

  applyProxySettings();

  connect(this, &QNetworkAccessManager::finished, this, [](QNetworkReply* reply) {
    if (!app()->isDebug()) return;
    qDebug() << "Response status:"
             << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "Response headers:";
    for (const auto& [name, value] : reply->rawHeaderPairs()) {
      qDebug() << u"%1: %2"_s.arg(name).arg(value);
    }
  });
}

void NetworkAccessManager::applyProxySettings() {
  setProxy(buildProxy());
}

QHttpHeaders NetworkAccessManager::commonHeaders() {
  QHttpHeaders headers;

  static const auto userAgentString = []() {
    return u"%1/%2.%3"_s.arg(TAIGA_APP_NAME).arg(TAIGA_VERSION_MAJOR).arg(TAIGA_VERSION_MINOR);
  };
  headers.append(QHttpHeaders::WellKnownHeader::UserAgent, userAgentString());

  return headers;
}

bool isDdosProtectionActive(const QRestReply& reply) {
  const auto server = reply.networkReply()->rawHeader("Server").toLower();

  switch (reply.httpStatus()) {
    case 403:
      return server.startsWith("ddos-guard");
    case 429:
    case 503:
      return server.startsWith("cloudflare");
    default:
      return false;
  }
}

}  // namespace taiga
