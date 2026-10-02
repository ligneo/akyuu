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

#include "release_selection.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QUrl>
#include <optional>

namespace akyuu {
namespace {

std::optional<semaver::Version> releaseVersion(const QString& tag) {
  auto versionText = tag;
  if (versionText.startsWith(u'v')) versionText.remove(0, 1);
  if (versionText.isEmpty()) return std::nullopt;

  semaver::Version version{versionText.toStdString()};
  if (version.to_string() != versionText.toStdString()) return std::nullopt;
  return version;
}

bool validReleasePage(const QString& page, const QString& tag) {
  const QUrl url{page};
  if (!url.isValid() || url.scheme() != u"https" || url.host() != u"github.com" ||
      !url.userInfo().isEmpty() || url.port(-1) != -1 || !url.query().isEmpty() ||
      !url.fragment().isEmpty()) {
    return false;
  }

  const auto path = url.path(QUrl::FullyDecoded);
  return path == QStringLiteral("/ligneo/akyuu/releases/tag/") + tag;
}

std::vector<ReleaseAsset> releaseAssets(const QJsonArray& values, const QString& tag,
                                        const semaver::Version& version) {
  std::vector<ReleaseAsset> assets;
  const auto versionText = QString::fromStdString(version.to_string());
  auto archVersion = versionText;
  archVersion.remove(u'-');
  const QRegularExpression archName{
      QStringLiteral("^akyuu-%1-[1-9][0-9]*-(x86_64|arm64)\\.pkg\\.tar\\.zst$")
          .arg(QRegularExpression::escape(archVersion))};
  const QRegularExpression digestPattern{QStringLiteral("^sha256:([0-9a-fA-F]{64})$")};
  for (const auto& value : values) {
    const auto object = value.toObject();
    if (object[QStringLiteral("state")].toString() != u"uploaded") continue;
    const auto digest = digestPattern.match(object[QStringLiteral("digest")].toString());
    if (!digest.hasMatch()) continue;
    ReleaseAsset asset{
        .name = object[QStringLiteral("name")].toString(),
        .url = QUrl{object[QStringLiteral("browser_download_url")].toString()},
        .size = object[QStringLiteral("size")].toInteger(-1),
        .sha256 = QByteArray::fromHex(digest.captured(1).toLatin1()),
        .target = {},
    };
    if (!validReleaseAsset(asset, tag)) continue;
    const auto arch = archName.match(asset.name);
    if (arch.hasMatch()) {
      asset.target = {UpdatePlatform::Linux, arch.captured(1), PackageFormat::Arch};
    } else {
      for (const auto& architecture : {QStringLiteral("x86_64"), QStringLiteral("arm64")}) {
        const auto stem = QStringLiteral("akyuu-%1-%2").arg(versionText, architecture);
        if (asset.name == stem + u".AppImage") {
          asset.target = {UpdatePlatform::Linux, architecture, PackageFormat::AppImage};
        } else if (asset.name == stem + u".deb") {
          asset.target = {UpdatePlatform::Linux, architecture, PackageFormat::Deb};
        } else if (asset.name == stem + u".rpm") {
          asset.target = {UpdatePlatform::Linux, architecture, PackageFormat::Rpm};
        } else if (asset.name == stem + u"-setup.exe") {
          asset.target = {UpdatePlatform::Windows, architecture, PackageFormat::WindowsInstaller};
        } else if (asset.name == stem + u".dmg") {
          asset.target = {UpdatePlatform::MacOS, architecture, PackageFormat::MacDiskImage};
        }
      }
    }
    if (asset.target.platform != UpdatePlatform::Unknown) assets.push_back(std::move(asset));
  }
  return assets;
}

}  // namespace

ReleaseSelection selectRelease(const QByteArray& response, const semaver::Version& current) {
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(response, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray())
    return {.status = ReleaseSelection::Status::InvalidResponse};

  const auto releases = document.array();
  if (releases.isEmpty()) return {.status = ReleaseSelection::Status::NoReleases};

  std::optional<ReleaseSelection> newest;
  for (const auto& value : releases) {
    if (!value.isObject()) continue;
    const auto release = value.toObject();
    if (release[QStringLiteral("draft")].toBool(true)) continue;

    const auto tagValue = release[QStringLiteral("tag_name")];
    const auto prereleaseValue = release[QStringLiteral("prerelease")];
    const auto pageValue = release[QStringLiteral("html_url")];
    if (!tagValue.isString() || !prereleaseValue.isBool() || !pageValue.isString()) continue;

    const auto tag = tagValue.toString();
    const auto version = releaseVersion(tag);
    if (!version ||
        (current.prerelease.empty() &&
         (prereleaseValue.toBool() || !version->prerelease.empty())) ||
        !validReleasePage(pageValue.toString(), tag)) {
      continue;
    }

    if (!newest || newest->version < *version) {
      newest = ReleaseSelection{
          .status = ReleaseSelection::Status::UpToDate,
          .version = *version,
          .tag = tag,
          .page = pageValue.toString(),
          .assets = releaseAssets(release[QStringLiteral("assets")].toArray(), tag, *version),
      };
    }
  }

  if (!newest) return {.status = ReleaseSelection::Status::NoCompatibleRelease};
  newest->status = newest->version > current ? ReleaseSelection::Status::UpdateAvailable
                                             : ReleaseSelection::Status::UpToDate;
  return *newest;
}

}  // namespace akyuu
