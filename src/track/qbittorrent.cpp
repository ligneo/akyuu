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

#include "qbittorrent.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <chrono>

#include "base/form.hpp"

// qBittorrent's Web API:
// https://github.com/qbittorrent/qBittorrent/wiki/WebUI-API-(qBittorrent-5.0)

namespace track {

namespace {

using namespace Qt::StringLiterals;

QNetworkRequest createRequest(const QBittorrentServer& server, const QString& path) {
  // A base without a trailing slash would lose its last segment, e.g. behind a reverse proxy
  auto base = server.url;
  if (!base.path().endsWith(u'/')) base.setPath(base.path() + u'/');

  QNetworkRequest request{base.resolved(QUrl{path})};
  // qBittorrent turns away requests whose referrer is another site
  request.setRawHeader("Referer", base.toEncoded());
  return request;
}

int httpStatus(const QNetworkReply* reply) {
  return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

QString networkError(const QNetworkReply* reply) {
  return QCoreApplication::translate("track::QBittorrent", "Could not reach qBittorrent: %1")
      .arg(reply->errorString());
}

// qBittorrent 5 answers with the number of torrents added, earlier versions with "Ok." or "Fails."
bool isAdded(const QByteArray& body) {
  if (body.trimmed() == "Fails.") return false;
  const auto json = QJsonDocument::fromJson(body);
  if (!json.isObject()) return true;
  return json.object().value("success_count"_L1).toInt() +
             json.object().value("pending_count"_L1).toInt() >
         0;
}

}  // namespace

QBittorrent::QBittorrent(QObject* parent) : QObject(parent) {
  manager_.setTransferTimeout(std::chrono::seconds{30});
}

void QBittorrent::add(const QBittorrentServer& server, const QString& target, const QString& folder,
                      const QString& category, callback_t callback) {
  add(server, target, folder, category, std::move(callback), true);
}

void QBittorrent::add(const QBittorrentServer& server, const QString& target, const QString& folder,
                      const QString& category, callback_t callback, const bool retry) {
  auto multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

  const auto addField = [multiPart](const QString& name, const QByteArray& value,
                                    const QString& fileName = {}) {
    QHttpPart part;
    auto disposition = u"form-data; name=\"%1\""_s.arg(name);
    if (!fileName.isEmpty()) disposition += u"; filename=\"%1\""_s.arg(fileName);
    part.setHeader(QNetworkRequest::ContentDispositionHeader, disposition);
    if (!fileName.isEmpty()) {
      part.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-bittorrent");
    }
    part.setBody(value);
    multiPart->append(part);
  };

  if (target.startsWith(u"magnet:")) {
    addField(u"urls"_s, target.toUtf8());
  } else {
    QFile file(target);
    if (!file.open(QIODevice::ReadOnly)) {
      delete multiPart;
      callback(tr("Could not read the torrent file."));
      return;
    }
    // The name only needs to look like a torrent file, and stays clear of quotes this way
    addField(u"torrents"_s, file.readAll(), u"file.torrent"_s);
  }
  if (!folder.isEmpty()) addField(u"savepath"_s, folder.toUtf8());
  if (!category.isEmpty()) addField(u"category"_s, category.toUtf8());

  auto reply = manager_.post(createRequest(server, u"api/v2/torrents/add"_s), multiPart);
  multiPart->setParent(reply);

  connect(reply, &QNetworkReply::finished, this,
          [this, reply, server, target, folder, category, callback, retry]() {
            reply->deleteLater();
            const int status = httpStatus(reply);

            // Signing in happens only when the session is missing or has expired
            if (status == 403 && retry) {
              login(server,
                    [this, server, target, folder, category, callback](const QString& error) {
                      if (!error.isEmpty()) {
                        callback(error);
                        return;
                      }
                      add(server, target, folder, category, callback, false);
                    });
              return;
            }

            if (status == 0) {
              callback(networkError(reply));
              return;
            }
            if (status == 415) {
              callback(tr("qBittorrent could not read the torrent file."));
              return;
            }
            if (status < 200 || status >= 300 || !isAdded(reply->readAll())) {
              callback(tr("qBittorrent did not add the torrent. It may be in its list already."));
              return;
            }
            callback({});
          });
}

void QBittorrent::test(const QBittorrentServer& server, test_callback_t callback) {
  login(server, [this, server, callback](const QString& error) {
    if (!error.isEmpty()) {
      callback({}, error);
      return;
    }
    auto reply = manager_.get(createRequest(server, u"api/v2/app/version"_s));
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
      reply->deleteLater();
      if (httpStatus(reply) != 200) {
        callback({}, networkError(reply));
        return;
      }
      callback(QString::fromUtf8(reply->readAll()).trimmed(), {});
    });
  });
}

void QBittorrent::login(const QBittorrentServer& server, callback_t callback) {
  auto request = createRequest(server, u"api/v2/auth/login"_s);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

  const base::FormFields body{{
      {u"username"_s, server.username},
      {u"password"_s, server.password},
  }};

  auto reply = manager_.post(request, base::formUrlEncode(body));

  connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
    reply->deleteLater();
    const int status = httpStatus(reply);

    if (status == 0) {
      callback(networkError(reply));
      return;
    }
    // qBittorrent 5 answers 401, earlier versions "Fails."
    if (status == 401 || reply->readAll().trimmed() == "Fails.") {
      callback(tr("qBittorrent did not accept the username and password."));
      return;
    }
    // Too many failed attempts get the address banned for a while
    if (status == 403) {
      callback(
          tr("qBittorrent refused to sign in after too many failed attempts. Try again later."));
      return;
    }
    if (status < 200 || status >= 300) {
      callback(tr("qBittorrent could not sign in (HTTP %1).").arg(status));
      return;
    }
    callback({});
  });
}

}  // namespace track
