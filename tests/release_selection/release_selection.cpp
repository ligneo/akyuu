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

#include "akyuu/release_selection.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <cstdlib>
#include <iostream>

namespace {

using Status = akyuu::ReleaseSelection::Status;

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

QJsonObject release(const QString& tag, bool prerelease, bool draft = false,
                    const QString& page = {}) {
  return {
      {QStringLiteral("tag_name"), tag},
      {QStringLiteral("prerelease"), prerelease},
      {QStringLiteral("draft"), draft},
      {QStringLiteral("html_url"),
       page.isEmpty() ? QStringLiteral("https://github.com/ligneo/akyuu/releases/tag/") + tag
                      : page},
  };
}

QByteArray response(const QJsonArray& releases) {
  return QJsonDocument{releases}.toJson(QJsonDocument::Compact);
}

void testPrereleaseSelectsHighestSemver() {
  const auto releases = QJsonArray{
      release(QStringLiteral("v0.1.0-beta.2"), true),
      release(QStringLiteral("v0.1.0-beta.10"), true),
      release(QStringLiteral("v0.1.0-beta.99"), true, true),
      release(QStringLiteral("not-a-version"), true),
  };
  const auto selected = akyuu::selectRelease(response(releases), semaver::Version{"0.1.0-beta.1"});

  require(selected.status == Status::UpdateAvailable, "beta channel should find an update");
  require(selected.tag == QStringLiteral("v0.1.0-beta.10"),
          "beta.10 should sort after beta.2 and drafts or malformed tags should be skipped");
}

void testPrereleaseCurrentAcceptsStableFinal() {
  const auto releases = QJsonArray{
      release(QStringLiteral("v0.1.0-beta.2"), true),
      release(QStringLiteral("v0.1.0"), false),
  };
  const auto selected = akyuu::selectRelease(response(releases), semaver::Version{"0.1.0-beta.1"});

  require(selected.status == Status::UpdateAvailable, "beta current should accept the final release");
  require(selected.tag == QStringLiteral("v0.1.0"), "a final release should outrank its beta versions");
}

void testStableSelectsOnlyStableReleases() {
  const auto releases = QJsonArray{
      release(QStringLiteral("v0.2.0-beta.1"), true),
      release(QStringLiteral("v0.1.1"), false),
      release(QStringLiteral("v0.2.0"), false, true),
      release(QStringLiteral("v0.1.0"), false),
  };
  const auto selected = akyuu::selectRelease(response(releases), semaver::Version{"0.1.0"});

  require(selected.status == Status::UpdateAvailable, "stable channel should find stable updates");
  require(selected.tag == QStringLiteral("v0.1.1"),
          "stable channel must skip prereleases and drafts");
}

void testStableCurrentDoesNotOfferFutureBeta() {
  const auto releases = QJsonArray{
      release(QStringLiteral("v0.2.0-beta.1"), true),
      release(QStringLiteral("v0.1.0"), false),
  };
  const auto selected = akyuu::selectRelease(response(releases), semaver::Version{"0.1.0"});

  require(selected.status == Status::UpToDate, "stable current should ignore future beta releases");
  require(selected.tag == QStringLiteral("v0.1.0"), "stable current should select its latest stable");
}

void testStableCurrentSkipsBetaTagEvenIfApiFlagIsFalse() {
  const auto releases = QJsonArray{
      release(QStringLiteral("v0.2.0-beta.1"), false),
      release(QStringLiteral("v0.1.0"), false),
  };
  const auto selected = akyuu::selectRelease(response(releases), semaver::Version{"0.1.0"});

  require(selected.status == Status::UpToDate,
          "stable channel should reject prerelease tags even when GitHub marks them stable");
  require(selected.tag == QStringLiteral("v0.1.0"), "stable channel should keep the stable tag");
}

void testMalformedPagesAndTagsAreSkipped() {
  const auto releases = QJsonArray{
      release(QStringLiteral("v0.3.0"), false, false,
              QStringLiteral("javascript:alert(1)")),
      release(QStringLiteral("v0.2.0-beta_1"), true),
  };
  const auto selected = akyuu::selectRelease(response(releases), semaver::Version{"0.1.0-beta.1"});

  require(selected.status == Status::NoCompatibleRelease,
          "malformed versions and untrusted release pages must not be selected");
}

void testEmptyAndInvalidResponses() {
  require(akyuu::selectRelease("[]", semaver::Version{"0.1.0"}).status == Status::NoReleases,
          "an empty release list should be reported as empty");
  require(akyuu::selectRelease("{broken", semaver::Version{"0.1.0"}).status ==
              Status::InvalidResponse,
          "malformed JSON should be reported as invalid");
  require(akyuu::selectRelease("{}", semaver::Version{"0.1.0"}).status ==
              Status::InvalidResponse,
          "an unexpected JSON shape should be reported as invalid");
}

void testFirstPageCanSelectFromOneHundredReleases() {
  QJsonArray releases;
  for (int index = 0; index < 100; ++index) {
    const auto tag = index == 99 ? QStringLiteral("v0.2.0") : QStringLiteral("v0.1.0");
    releases.append(release(tag, false));
  }

  const auto selected = akyuu::selectRelease(response(releases), semaver::Version{"0.1.0"});
  require(selected.status == Status::UpdateAvailable && selected.tag == QStringLiteral("v0.2.0"),
          "the selector should handle all 100 releases returned on the API's first page");
}

void testInstallationTargets() {
  using Platform = akyuu::UpdatePlatform;
  using Format = akyuu::PackageFormat;
  const auto check = [](const char* kernel, const char* architecture, const char* format,
                        Platform platform, const char* normalized, Format package) {
    const auto target = akyuu::updateTarget(QString::fromLatin1(kernel),
        QString::fromLatin1(architecture), QString::fromLatin1(format));
    require(target.platform == platform && target.architecture == QString::fromLatin1(normalized) &&
                target.format == package, "installation target must follow the build contract");
  };
  check("linux", "x86_64", "arch", Platform::Linux, "x86_64", Format::Arch);
  check("linux", "aarch64", "appimage", Platform::Linux, "arm64", Format::AppImage);
  check("linux", "x86_64", "source", Platform::Linux, "x86_64", Format::AppImage);
  check("winnt", "x86_64", "source", Platform::Windows, "x86_64", Format::WindowsInstaller);
  check("darwin", "arm64", "source", Platform::MacOS, "arm64", Format::MacDiskImage);
  check("unknown", "x86_64", "source", Platform::Unknown, "x86_64", Format::AppImage);
  qputenv("APPIMAGE", "/unrelated-launcher.AppImage");
  check("linux", "x86_64", "arch", Platform::Linux, "x86_64", Format::Arch);
  const auto target = akyuu::updateTarget();
  const auto expected = akyuu::updateTarget(QSysInfo::kernelType(),
      QSysInfo::currentCpuArchitecture(), QStringLiteral("source"));
  require(target.platform == expected.platform && target.architecture == expected.architecture &&
              target.format == expected.format, "runtime target must use the same policy");
  qunsetenv("APPIMAGE");
}

void testPlatformPackages() {
  const QString tag = QStringLiteral("v0.1.0-beta.3");
  auto item = release(tag, true);
  QJsonArray assets;
  const auto add = [&assets, &tag](const QString& name, const QString& digest = QString(64, u'a'),
                                   const QString& host = QStringLiteral("github.com")) {
    assets.append(QJsonObject{
        {QStringLiteral("name"), name},
        {QStringLiteral("state"), QStringLiteral("uploaded")},
        {QStringLiteral("size"), 100},
        {QStringLiteral("digest"), QStringLiteral("sha256:") + digest},
        {QStringLiteral("browser_download_url"),
         QStringLiteral("https://%1/ligneo/akyuu/releases/download/%2/%3").arg(host, tag, name)}});
  };
  add(QStringLiteral("akyuu-0.1.0-beta.3-x86_64.AppImage"));
  add(QStringLiteral("akyuu-0.1.0-beta.3-arm64.AppImage"));
  add(QStringLiteral("akyuu-0.1.0beta.3-1-x86_64.pkg.tar.zst"));
  add(QStringLiteral("akyuu-0.1.0-beta.3-x86_64-setup.exe"));
  add(QStringLiteral("akyuu-0.1.0-beta.3-arm64.dmg"));
  add(QStringLiteral("akyuu-0.1.0-beta.3-source.tar.xz"));
  add(QStringLiteral("akyuu-0.1.0-beta.3-arm64-setup.exe"), QStringLiteral("broken"));
  add(QStringLiteral("akyuu-0.1.0-beta.3-x86_64.dmg"), QString(64, u'a'),
      QStringLiteral("example.com"));
  item.insert(QStringLiteral("assets"), assets);
  const auto selected = akyuu::selectRelease(response({item}), semaver::Version{"0.1.0-beta.2"});
  require(selected.assets.size() == 5,
          "source archives and untrusted assets must not become install packages");
  using Platform = akyuu::UpdatePlatform;
  using Format = akyuu::PackageFormat;
  for (const auto& target :
       {akyuu::UpdateTarget{Platform::Linux, QStringLiteral("x86_64"), Format::AppImage},
        akyuu::UpdateTarget{Platform::Linux, QStringLiteral("arm64"), Format::AppImage},
        akyuu::UpdateTarget{Platform::Linux, QStringLiteral("x86_64"), Format::Arch},
        akyuu::UpdateTarget{Platform::Windows, QStringLiteral("x86_64"), Format::WindowsInstaller},
        akyuu::UpdateTarget{Platform::MacOS, QStringLiteral("arm64"), Format::MacDiskImage}}) {
    const auto package = akyuu::selectPackage(selected.assets, target);
    require(package && package->target.platform == target.platform &&
                package->target.architecture == target.architecture &&
                package->target.format == target.format,
            "the package must match the full installation target");
  }
  require(!akyuu::selectPackage(selected.assets, {Platform::Windows, QStringLiteral("arm64"),
                                                  Format::WindowsInstaller}),
          "an unavailable architecture must never receive another platform's installer");
  auto duplicates = selected.assets;
  duplicates.push_back(duplicates.front());
  require(!akyuu::selectPackage(duplicates,
                                {Platform::Linux, QStringLiteral("x86_64"), Format::AppImage}),
          "ambiguous packages must not be selected");
}

}  // namespace

int main() {
  testPrereleaseSelectsHighestSemver();
  testPrereleaseCurrentAcceptsStableFinal();
  testStableSelectsOnlyStableReleases();
  testStableCurrentDoesNotOfferFutureBeta();
  testStableCurrentSkipsBetaTagEvenIfApiFlagIsFalse();
  testMalformedPagesAndTagsAreSkipped();
  testEmptyAndInvalidResponses();
  testFirstPageCanSelectFromOneHundredReleases();
  testInstallationTargets();
  testPlatformPackages();
  std::cout << "Passed release selection tests.\n";
}
