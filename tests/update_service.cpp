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

#include "akyuu/update_service.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

struct Response {
  QByteArray body;
  QByteArray link;
  qint64 declaredSize = -1;
  bool delayed = false;
};

class Server : public QTcpServer {
public:
  std::vector<Response> responses;
  std::size_t index = 0;
  Server() {
    require(listen(QHostAddress::LocalHost), "local fixture server must listen");
    connect(this, &QTcpServer::newConnection, this, [this]() {
      const auto socket = nextPendingConnection();
      const auto request = std::make_shared<QByteArray>();
      connect(socket, &QTcpSocket::readyRead, socket, [this, socket, request]() {
        request->append(socket->readAll());
        if (!request->contains("\r\n\r\n")) return;
        socket->disconnect(socket, &QTcpSocket::readyRead, nullptr, nullptr);
        require(index < responses.size(), "unexpected network request");
        const auto response = responses[index++];
        const auto length =
            response.declaredSize < 0 ? response.body.size() : response.declaredSize;
        QByteArray headers =
            "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(length) + "\r\n";
        if (!response.link.isEmpty()) headers += "Link: " + response.link + "\r\n";
        socket->write(headers + "Connection: close\r\n\r\n");
        if (response.delayed) {
          socket->write(response.body.left(3));
          QTimer::singleShot(200, socket, [socket, response]() {
            socket->write(response.body.mid(3));
            socket->disconnectFromHost();
          });
        } else {
          socket->write(response.body);
          socket->disconnectFromHost();
        }
      });
      connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    });
  }
};

class Network : public QNetworkAccessManager {
public:
  Server server;
  std::vector<QUrl> requests;

protected:
  QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                               QIODevice* data) override {
    requests.push_back(request.url());
    auto mapped = request;
    QUrl local{QStringLiteral("http://127.0.0.1")};
    local.setPort(server.serverPort());
    local.setPath(request.url().path());
    local.setQuery(request.url().query());
    mapped.setUrl(local);
    return QNetworkAccessManager::createRequest(operation, mapped, data);
  }
};

QByteArray releases(const QString& version) {
  return QJsonDocument{
      QJsonArray{
          QJsonObject{{QStringLiteral("tag_name"), version},
                      {QStringLiteral("draft"), false},
                      {QStringLiteral("prerelease"), false},
                      {QStringLiteral("html_url"),
                       QStringLiteral("https://github.com/ligneo/akyuu/releases/tag/") + version}}}}
      .toJson();
}

void wait(QEventLoop& loop) {
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, &loop,
                   []() { require(false, "asynchronous test timed out"); });
  timeout.start(5000);
  loop.exec();
}

void testPaginationAndReuse() {
  Network network;
  network.server.responses = {
      {releases(QStringLiteral("v0.1.0")),
       "<https://api.github.com/repos/ligneo/akyuu/releases?page=2>; rel=\"next\""},
      {releases(QStringLiteral("v0.2.0"))},
      {"broken"},
      {releases(QStringLiteral("v0.1.0"))},
  };
  akyuu::UpdateService service{&network};
  QEventLoop loop;
  akyuu::ReleaseSelection selected;
  int errors = 0;
  QObject::connect(&service, &akyuu::UpdateService::checked, &loop, [&](auto result) {
    selected = result;
    loop.quit();
  });
  QObject::connect(&service, &akyuu::UpdateService::failed, &loop, [&](auto) {
    ++errors;
    loop.quit();
  });
  require(service.check(semaver::Version{"0.1.0"}), "a check must start");
  require(!service.check(semaver::Version{"0.1.0"}), "concurrent checks must not interleave");
  wait(loop);
  require(selected.tag == QStringLiteral("v0.2.0") && network.requests.size() == 2,
          "a release on the second page must be selected");
  require(network.requests.back().query().contains(QStringLiteral("page=2")),
          "pagination must request the next fixed API page");
  service.check(semaver::Version{"0.1.0"});
  wait(loop);
  require(errors == 1 && !service.isBusy(), "a malformed response must finish with an error");
  service.check(semaver::Version{"0.1.0"});
  wait(loop);
  require(selected.status == akyuu::ReleaseSelection::Status::UpToDate,
          "a service must be reusable after failure");
}

void testDownload(const Response& response, bool validHash, bool cancel, bool success) {
  Network network;
  network.server.responses = {response};
  akyuu::UpdateService service{&network};
  QTemporaryDir directory;
  require(directory.isValid(), "test directory must exist");
  const auto path = directory.filePath(QStringLiteral("update.AppImage"));
  QFile original{path};
  require(original.open(QIODevice::WriteOnly), "original destination must open");
  original.write("previous file");
  original.close();
  const QByteArray expected = "verified package bytes";
  auto hash = QCryptographicHash::hash(expected, QCryptographicHash::Sha256);
  if (!validHash) hash[0] ^= 1;
  const akyuu::ReleaseAsset asset{
      .name = QStringLiteral("akyuu-0.2.0-x86_64.AppImage"),
      .url = QUrl{QStringLiteral(
          "https://github.com/ligneo/akyuu/releases/download/v0.2.0/akyuu-0.2.0-x86_64.AppImage")},
      .size = expected.size(),
      .sha256 = hash,
      .target = {akyuu::UpdatePlatform::Linux, QStringLiteral("x86_64"),
                 akyuu::PackageFormat::AppImage}};
  QEventLoop loop;
  bool downloaded = false, failed = false, cancelled = false;
  QObject::connect(&service, &akyuu::UpdateService::downloaded, &loop, [&](auto) {
    downloaded = true;
    loop.quit();
  });
  QObject::connect(&service, &akyuu::UpdateService::failed, &loop, [&](auto) {
    failed = true;
    loop.quit();
  });
  QObject::connect(&service, &akyuu::UpdateService::cancelled, &loop, [&]() {
    cancelled = true;
    loop.quit();
  });
  if (cancel)
    QObject::connect(&service, &akyuu::UpdateService::progress, &loop,
                     [&](auto, auto) { service.cancel(); });
  require(service.download(asset, QStringLiteral("v0.2.0"), path), "a valid download must start");
  wait(loop);
  require(downloaded == success, "only a verified download may report success");
  require(cancel ? cancelled && !failed : success || failed,
          "download failure and cancellation must be distinct");
  require(original.open(QIODevice::ReadOnly), "destination must remain readable");
  require(original.readAll() == (success ? expected : QByteArray{"previous file"}),
          "failed, truncated, oversized or cancelled downloads must preserve the existing file");
  if (success)
    require(original.permissions() & QFileDevice::ExeOwner, "an AppImage must be executable");
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application{argc, argv};
  testPaginationAndReuse();
  const QByteArray body = "verified package bytes";
  testDownload({body}, true, false, true);
  testDownload({body}, false, false, false);
  testDownload({body + "extra"}, true, false, false);
  testDownload({body.left(5), {}, body.size()}, true, false, false);
  testDownload({body, {}, -1, true}, true, true, false);
  std::cout << "Passed paginated update checks and atomic download tests.\n";
}
