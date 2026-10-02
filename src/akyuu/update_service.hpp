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

#pragma once

#include <QCryptographicHash>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QSaveFile>
#include <memory>

#include "release_selection.hpp"

namespace akyuu {

class UpdateService final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(UpdateService)

public:
  explicit UpdateService(QNetworkAccessManager* network, QObject* parent = nullptr);
  ~UpdateService() override;

  bool check(const semaver::Version& current);
  bool download(const ReleaseAsset& asset, const QString& tag, const QString& path);
  bool isBusy() const;

public slots:
  void cancel();

signals:
  void checked(akyuu::ReleaseSelection release);
  void progress(qint64 received, qint64 total);
  void downloaded(QString path);
  void failed(QString message);
  void cancelled();

private:
  void checkPage();
  void readDownload();
  void finishDownload();
  void fail(const QString& message);
  QNetworkReply* get(const QUrl& url);

  QNetworkAccessManager* network_;
  QPointer<QNetworkReply> reply_;
  semaver::Version current_{};
  QJsonArray releases_;
  int page_ = 0;
  std::unique_ptr<QSaveFile> file_;
  QCryptographicHash hash_{QCryptographicHash::Sha256};
  ReleaseAsset asset_;
  qint64 received_ = 0;
  QString error_;
  bool cancelled_ = false;
};

}  // namespace akyuu
