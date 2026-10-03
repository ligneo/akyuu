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

#include <QGuiApplication>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSslSocket>
#include <QTimer>
#include <iostream>

int main(int argc, char** argv) {
  QGuiApplication app{argc, argv};
  const auto formats = QImageReader::supportedImageFormats();
  for (const auto& format : {"png", "jpeg", "svg"}) {
    if (!formats.contains(format)) {
      std::cerr << "Missing image plugin: " << format << '\n';
      return 1;
    }
  }
  auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
  database.setDatabaseName(QStringLiteral(":memory:"));
  if (!database.open()) {
    std::cerr << "The deployed SQLite driver could not open an in-memory database.\n";
    return 2;
  }
  QSqlQuery query;
  if (!query.exec(QStringLiteral("SELECT 42")) || !query.next() || query.value(0).toInt() != 42)
    return 3;
  if (!QSslSocket::supportsSsl()) {
    std::cerr << "The deployed runtime has no TLS backend.\n";
    return 4;
  }
  if (app.arguments().contains(QStringLiteral("--network"))) {
    QNetworkAccessManager network;
    auto* reply = network.get(QNetworkRequest{QUrl{QStringLiteral("https://github.com/ligneo/akyuu/releases")}});
    QTimer::singleShot(30000, &app, [&] { app.exit(5); });
    QObject::connect(reply, &QNetworkReply::finished, &app, [&] {
      if (reply->error() != QNetworkReply::NoError) {
        std::cerr << "HTTPS runtime check failed: " << reply->errorString().toStdString()
                  << " (HTTP " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                  << ")\n";
      }
      app.exit(reply->error() == QNetworkReply::NoError ? 0 : 6);
    });
    if (const auto result = app.exec()) return result;
  }
  std::cout << "Passed deployed SQLite, TLS and PNG/JPEG/SVG checks.\n";
}
