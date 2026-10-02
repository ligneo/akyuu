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

#include "update_service.hpp"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkRequest>

namespace akyuu {

UpdateService::UpdateService(QNetworkAccessManager* network, QObject* parent)
    : QObject{parent}, network_{network} {}

UpdateService::~UpdateService() {
  if (reply_) {
    reply_->disconnect(this);
    reply_->abort();
    reply_->deleteLater();
  }
}

bool UpdateService::isBusy() const {
  return !reply_.isNull();
}

QNetworkReply* UpdateService::get(const QUrl& url) {
  QNetworkRequest request{url};
  request.setRawHeader("User-Agent", "Akyuu");
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
  request.setTransferTimeout(30000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  auto reply = network_->get(request);
  reply->setReadBufferSize(1024 * 1024);
  return reply;
}

bool UpdateService::check(const semaver::Version& current) {
  if (isBusy()) return false;
  current_ = current;
  releases_ = {};
  page_ = 0;
  cancelled_ = false;
  checkPage();
  return true;
}

void UpdateService::checkPage() {
  ++page_;
  reply_ = get(
      QUrl{QStringLiteral("https://api.github.com/repos/ligneo/akyuu/releases?per_page=100&page=%1")
               .arg(page_)});
  const auto reply = reply_.data();
  auto body = std::make_shared<QByteArray>();
  connect(reply, &QNetworkReply::readyRead, this, [reply, body]() {
    body->append(reply->readAll());
    if (body->size() > 2 * 1024 * 1024) reply->abort();
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply, body]() {
    body->append(reply->readAll());
    const auto error = reply->error();
    const auto message = reply->errorString();
    const bool next = reply->rawHeader("Link").contains("rel=\"next\"");
    reply->deleteLater();
    reply_.clear();
    if (cancelled_) {
      emit cancelled();
      return;
    }
    if (body->size() > 2 * 1024 * 1024) {
      fail(tr("Release data is too large."));
      return;
    }
    if (error != QNetworkReply::NoError) {
      fail(message);
      return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(*body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
      fail(tr("The server returned invalid release data."));
      return;
    }
    for (const auto& value : document.array()) releases_.append(value);
    if (next) {
      if (page_ == 10) {
        fail(tr("The release list is incomplete. Open the downloads page."));
        return;
      }
      checkPage();
      return;
    }
    const auto selected = selectRelease(QJsonDocument{releases_}.toJson(), current_);
    releases_ = {};
    emit checked(selected);
  });
}

bool UpdateService::download(const ReleaseAsset& asset, const QString& tag, const QString& path) {
  if (isBusy()) return false;
  if (!validReleaseAsset(asset, tag)) {
    fail(tr("The download information is invalid."));
    return false;
  }
  file_ = std::make_unique<QSaveFile>(path);
  if (!file_->open(QIODevice::WriteOnly)) {
    const auto message = file_->errorString();
    fail(message);
    return false;
  }
  asset_ = asset;
  received_ = 0;
  error_.clear();
  cancelled_ = false;
  hash_.reset();
  reply_ = get(asset.url);
  connect(reply_, &QNetworkReply::readyRead, this, &UpdateService::readDownload);
  connect(reply_, &QNetworkReply::finished, this, &UpdateService::finishDownload);
  return true;
}

void UpdateService::readDownload() {
  if (!error_.isEmpty() || cancelled_) return;
  while (reply_ && reply_->bytesAvailable() > 0) {
    const auto data = reply_->read(64 * 1024);
    received_ += data.size();
    if (received_ > asset_.size) {
      error_ = tr("The download is larger than expected.");
    } else if (file_->write(data) != data.size()) {
      error_ = file_->errorString();
    } else {
      hash_.addData(data);
      emit progress(received_, asset_.size);
    }
    if (!error_.isEmpty()) {
      reply_->abort();
      return;
    }
  }
}

void UpdateService::finishDownload() {
  readDownload();
  if (!reply_) return;
  const auto reply = reply_.data();
  const auto networkError = reply->error();
  const auto networkMessage = reply->errorString();
  reply->deleteLater();
  reply_.clear();
  if (cancelled_) {
    file_.reset();
    emit cancelled();
    return;
  }
  if (error_.isEmpty() && networkError != QNetworkReply::NoError) error_ = networkMessage;
  if (error_.isEmpty() && (received_ != asset_.size || hash_.result() != asset_.sha256)) {
    error_ = tr("The download failed its integrity check. Please try again.");
  }
  if (!error_.isEmpty()) {
    fail(error_);
    return;
  }
  if (asset_.target.format == PackageFormat::AppImage &&
      !file_->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                             QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                             QFileDevice::ExeGroup | QFileDevice::ReadOther |
                             QFileDevice::ExeOther)) {
    fail(file_->errorString());
    return;
  }
  const auto path = file_->fileName();
  if (!file_->commit()) {
    fail(file_->errorString());
    return;
  }
  file_.reset();
  emit downloaded(path);
}

void UpdateService::fail(const QString& message) {
  file_.reset();
  releases_ = {};
  emit failed(message);
}

void UpdateService::cancel() {
  if (!reply_) return;
  cancelled_ = true;
  reply_->abort();
}

}  // namespace akyuu
