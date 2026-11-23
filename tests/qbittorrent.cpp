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

#include "track/qbittorrent.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTimer>
#include <cstdlib>
#include <functional>
#include <iostream>

namespace {

using namespace Qt::StringLiterals;

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

struct Request {
  QByteArray method;
  QByteArray path;
  QByteArray headers;
  QByteArray body;
};

// Answers one request per connection, the way a handler decides
class FakeServer {
public:
  using handler_t = std::function<QByteArray(const Request&)>;

  explicit FakeServer(handler_t handler) : handler_(std::move(handler)) {
    require(server_.listen(QHostAddress::LocalHost), "The fake server listens");
    QObject::connect(&server_, &QTcpServer::newConnection, [this]() {
      auto socket = server_.nextPendingConnection();
      QObject::connect(socket, &QTcpSocket::readyRead, [this, socket]() { read(socket); });
    });
  }

  QUrl url() const {
    return QUrl{u"http://127.0.0.1:%1"_s.arg(server_.serverPort())};
  }

  QList<Request> requests;

private:
  void read(QTcpSocket* socket) {
    auto& buffer = buffers_[socket];
    buffer += socket->readAll();
    const auto end = buffer.indexOf("\r\n\r\n");
    if (end < 0) return;

    Request request;
    request.headers = buffer.left(end);
    const auto line = request.headers.left(request.headers.indexOf("\r\n")).split(' ');
    request.method = line.value(0);
    request.path = line.value(1);

    qsizetype length = 0;
    for (const auto& header : request.headers.split('\n')) {
      if (header.toLower().startsWith("content-length:"))
        length = header.mid(15).trimmed().toLongLong();
    }
    if (buffer.size() < end + 4 + length) return;
    request.body = buffer.mid(end + 4, length);

    requests.append(request);
    socket->write(handler_(request));
    socket->disconnectFromHost();
    buffers_.remove(socket);
  }

  QTcpServer server_;
  handler_t handler_;
  QHash<QTcpSocket*, QByteArray> buffers_;
};

QByteArray response(int status, const QByteArray& body = {}, const QByteArray& headers = {}) {
  return "HTTP/1.1 " + QByteArray::number(status) +
         " X\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n" +
         headers + "\r\n" + body;
}

QString wait(const std::function<void(track::QBittorrent::callback_t)>& call) {
  QEventLoop loop;
  QString result = u"timed out"_s;
  QTimer::singleShot(5000, &loop, &QEventLoop::quit);
  call([&](const QString& error) {
    result = error;
    loop.quit();
  });
  loop.exec();
  return result;
}

// qBittorrent 5: 204 and QBT_SID_<port> on sign-in, 403 without it, a JSON count on adding
QByteArray version5(const Request& request) {
  if (request.path == "/api/v2/auth/login") {
    if (request.body != "username=admin&password=a%2Bb%25") return response(401);
    return response(204, {}, "Set-Cookie: QBT_SID_8080=abc; HttpOnly; path=/\r\n");
  }
  if (request.path == "/api/v2/torrents/add") {
    if (!request.headers.contains("QBT_SID_8080=abc")) return response(403, "Forbidden");
    return response(
        200,
        R"({"added_torrent_ids":["x"],"failure_count":0,"pending_count":0,"success_count":1})");
  }
  return response(404);
}

// qBittorrent 4: "Ok." and SID on sign-in, "Ok." or "Fails." on adding
QByteArray version4(const Request& request) {
  if (request.path == "/api/v2/auth/login") {
    return response(200, "Ok.", "Set-Cookie: SID=xyz; HttpOnly; path=/\r\n");
  }
  if (request.path == "/api/v2/torrents/add") {
    if (!request.headers.contains("SID=xyz")) return response(403, "Forbidden");
    return response(200, request.body.contains("fails") ? "Fails." : "Ok.");
  }
  return response(404);
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);

  // Signs in when the session is missing, with a password that needs encoding, then adds
  {
    FakeServer server{version5};
    track::QBittorrent client;
    const track::QBittorrentServer settings{server.url(), u"admin"_s, u"a+b%"_s};
    const auto error = wait([&](auto callback) {
      client.add(settings, u"magnet:?xt=urn:btih:abc"_s, u"/downloads/Anime"_s, u"anime"_s,
                 callback);
    });
    require(error.isEmpty(), "A magnet link is added to qBittorrent 5");
    require(server.requests.size() == 3, "Adding signs in once and tries again");
    const auto& add = server.requests.last();
    require(add.headers.contains("Referer: " + server.url().toEncoded()), "The referrer is sent");
    require(add.body.contains("name=\"urls\"") && add.body.contains("magnet:?xt=urn:btih:abc"),
            "The magnet link is sent");
    require(add.body.contains("name=\"savepath\"") && add.body.contains("/downloads/Anime"),
            "The folder is sent");
    require(add.body.contains("name=\"category\"") && add.body.contains("anime"),
            "The category is sent");

    // The session is kept for the next one
    const auto again = wait([&](auto callback) {
      client.add(settings, u"magnet:?xt=urn:btih:def"_s, {}, {}, callback);
    });
    require(again.isEmpty() && server.requests.size() == 4, "The session is reused");
    require(!server.requests.last().body.contains("savepath"), "No folder is sent when empty");
  }

  // A wrong password is reported
  {
    FakeServer server{version5};
    track::QBittorrent client;
    const auto error = wait([&](auto callback) {
      client.add({server.url(), u"admin"_s, u"wrong"_s}, u"magnet:?xt=urn:btih:abc"_s, {}, {},
                 callback);
    });
    require(error.contains(u"username and password"_s), "A wrong password is reported");
  }

  // qBittorrent 4 and a torrent file
  {
    FakeServer server{version4};
    track::QBittorrent client;
    QTemporaryFile file;
    require(file.open(), "A torrent file is written");
    file.write("d8:announce4:test4:infod4:name4:testee");
    file.close();

    const auto error = wait([&](auto callback) {
      client.add({server.url(), u"admin"_s, u"secret"_s}, file.fileName(), {}, {}, callback);
    });
    require(error.isEmpty(), "A torrent file is added to qBittorrent 4");
    const auto& add = server.requests.last();
    require(add.body.contains("name=\"torrents\"; filename=\"file.torrent\"") &&
                add.body.contains("4:infod4:name"),
            "The torrent file is uploaded");

    const auto failed = wait([&](auto callback) {
      client.add({server.url(), u"admin"_s, u"secret"_s}, u"magnet:?fails"_s, {}, {}, callback);
    });
    require(!failed.isEmpty(), "\"Fails.\" is reported");
  }

  // A server that cannot be reached
  {
    QUrl url;
    {
      FakeServer server{version5};
      url = server.url();
    }
    track::QBittorrent client;
    const auto error = wait([&](auto callback) {
      client.add({url, u"admin"_s, u"a"_s}, u"magnet:?xt=urn:btih:abc"_s, {}, {}, callback);
    });
    require(error.contains(u"Could not reach"_s), "An unreachable server is reported");
  }

  return EXIT_SUCCESS;
}
