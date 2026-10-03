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

#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <functional>
#include <stdexcept>

#include "common.hpp"
#include "sources.hpp"

using namespace Qt::StringLiterals;

namespace release {
namespace {

using Files = QMap<QString, QByteArray>;

QByteArray digest(const QByteArray& data) {
  return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

Files fixture() {
  const auto data = QByteArray("corresponding source archive");
  const QJsonObject library{{u"version"_s, u"1"_s}, {u"binary"_s, u"liblibrary1"_s}};
  const auto inventory = QJsonDocument(QJsonObject{{u"library"_s, library}}).toJson();
  const auto notice = QByteArray("akyuu-0.1.0-beta.5-third-party-sources.tar");
  return {{u"licenses/NOTICE"_s, notice},
          {u"licenses/licenses/GPL-3"_s, "common license text"},
          {u"licenses/source-packages.json"_s, inventory},
          {u"third-party-sources/README"_s, notice},
          {u"third-party-sources/source-packages.json"_s, inventory},
          {u"third-party-sources/ubuntu/library_1.tar.xz"_s, data},
          {u"third-party-sources/ubuntu/library_1.dsc"_s,
           "Source: library\nVersion: 1\nChecksums-Sha256:\n " + digest(data) + " " +
               QByteArray::number(data.size()) + " library_1.tar.xz\n"}};
}

void manifest(Files& files) {
  QByteArray data;
  for (auto it = files.cbegin(); it != files.cend(); ++it) {
    if (it.key().startsWith(u"third-party-sources/") &&
        it.key() != u"third-party-sources/SHA256SUMS") {
      data += digest(it.value()) + "  " + it.key().mid(20).toUtf8() + '\n';
    }
  }
  files.insert(u"third-party-sources/SHA256SUMS"_s, data);
}

void octal(QByteArray& header, qsizetype offset, qsizetype length, quint64 value) {
  const auto text = QByteArray::number(value, 8).rightJustified(length - 1, '0');
  require(text.size() < length, u"Fixture tar integer overflow."_s);
  header.replace(offset, length, text + '\0');
}

void member(QFile& archive, const QString& name, const QByteArray& data, char type = '0') {
  const auto path = name.toUtf8();
  require(path.size() < 100, u"Fixture tar name is too long."_s);
  QByteArray header(512, '\0');
  header.replace(0, path.size(), path);
  octal(header, 100, 8, 0644);
  octal(header, 108, 8, 0);
  octal(header, 116, 8, 0);
  octal(header, 124, 12, data.size());
  octal(header, 136, 12, 0);
  header.replace(148, 8, QByteArray(8, ' '));
  header[156] = type;
  header.replace(257, 6, QByteArray("ustar\0", 6));
  header.replace(263, 2, "00");
  quint64 checksum = 0;
  for (const auto byte : header) checksum += static_cast<unsigned char>(byte);
  header.replace(148, 8, QByteArray::number(checksum, 8).rightJustified(6, '0') + '\0' + ' ');
  require(archive.write(header) == header.size() && archive.write(data) == data.size(),
          u"Cannot write fixture archive."_s);
  const QByteArray padding((512 - data.size() % 512) % 512, '\0');
  require(archive.write(padding) == padding.size(), u"Cannot pad fixture archive."_s);
}

QByteArray paxRecord(const QByteArray& key, const QByteArray& value) {
  const auto body = key + '=' + value + '\n';
  qsizetype size = body.size() + 2;
  for (;;) {
    const auto result = QByteArray::number(size) + ' ' + body;
    if (result.size() == size) return result;
    size = result.size();
  }
}

void archive(const QString& path, const Files& files, const QString& duplicate = {},
             const QString& special = {}, char type = '0', const QByteArray& specialData = {},
             const QByteArray& metadata = {}, bool sparse = false) {
  QFile output(path);
  require(output.open(QIODevice::WriteOnly), u"Cannot create fixture archive."_s);
  for (auto it = files.cbegin(); it != files.cend(); ++it) {
    if (it.key() == u"licenses/NOTICE" && !metadata.isEmpty()) {
      member(output, u"PaxHeaders/NOTICE"_s, metadata, 'x');
    }
    if (it.key() == u"licenses/NOTICE" && sparse) {
      auto mapping = QByteArray("1\n0\n") + QByteArray::number(it.value().size()) + '\n';
      mapping += QByteArray(512 - mapping.size(), '\0');
      member(output, it.key(), mapping + it.value());
    } else {
      member(output, it.key(), it.value());
    }
  }
  if (!duplicate.isEmpty()) member(output, duplicate, files.value(duplicate));
  if (!special.isEmpty()) member(output, special, specialData, type);
  require(output.write(QByteArray(1024, '\0')) == 1024, u"Cannot finish fixture archive."_s);
}

}  // namespace

int sourceTests() {
  QTemporaryDir directory;
  require(directory.isValid(), u"Cannot create source test directory."_s);
  const auto path = directory.filePath(u"companion.tar"_s);
  int count = 0;
  const auto check =
      [&](const QString& name, const std::function<void(Files&)>& mutate, bool valid = false,
          bool createManifest = true, const QString& duplicate = {}, const QString& special = {},
          char type = '0', const QByteArray& specialData = {}, const QByteArray& metadata = {}) {
        auto files = fixture();
        mutate(files);
        if (createManifest) manifest(files);
        archive(path, files, duplicate, special, type, specialData, metadata);
        bool accepted = true;
        QString error;
        try {
          verifySources(path, u"0.1.0-beta.5"_s);
        } catch (const std::exception& failure) {
          accepted = false;
          error = QString::fromUtf8(failure.what());
        }
        require(accepted == valid, u"Source companion test failed: %1 (%2)"_s.arg(name, error));
        ++count;
      };

  check(u"valid archive"_s, [](Files&) {}, true);
  check(u"stale inventory"_s,
        [](Files& files) { files[u"licenses/source-packages.json"_s] = "{}"; });
  check(u"nested licenses"_s,
        [](Files& files) { files[u"licenses/licenses/NOTICE"_s] = files[u"licenses/NOTICE"_s]; });
  check(u"stale notice"_s, [](Files& files) {
    files[u"licenses/NOTICE"_s] = "akyuu-0.1.0-beta.4-third-party-sources.tar";
  });
  check(
      u"unlisted source"_s, [](Files& files) { files[u"third-party-sources/SHA256SUMS"_s] = {}; },
      false, false);
  check(u"unreferenced source"_s, [](Files& files) {
    files[u"third-party-sources/ubuntu/stale-source.tar.xz"_s] = "stale source";
  });
  check(u"wrong source version"_s, [](Files& files) {
    files[u"third-party-sources/ubuntu/library_1.dsc"_s].replace("Version: 1", "Version: 2");
  });
  check(u"wrong descriptor hash"_s, [](Files& files) {
    files[u"third-party-sources/ubuntu/library_1.tar.xz"_s] = "changed source archive";
  });
  check(u"wrong descriptor size"_s, [](Files& files) {
    files[u"third-party-sources/ubuntu/library_1.dsc"_s].replace(" 28 library", " 29 library");
  });
  check(u"missing descriptor"_s,
        [](Files& files) { files.remove(u"third-party-sources/ubuntu/library_1.dsc"_s); });
  check(u"duplicate descriptor"_s, [](Files& files) {
    files[u"third-party-sources/ubuntu/duplicate.dsc"_s] =
        files[u"third-party-sources/ubuntu/library_1.dsc"_s];
  });
  check(u"descriptor path traversal"_s, [](Files& files) {
    files[u"third-party-sources/ubuntu/library_1.dsc"_s].replace("library_1.tar.xz",
                                                                 "../library_1.tar.xz");
  });
  check(
      u"manifest checksum mismatch"_s,
      [](Files& files) {
        manifest(files);
        auto& data = files[u"third-party-sources/SHA256SUMS"_s];
        data[0] = data[0] == '0' ? '1' : '0';
      },
      false, false);
  check(
      u"duplicate manifest entry"_s,
      [](Files& files) {
        manifest(files);
        auto& data = files[u"third-party-sources/SHA256SUMS"_s];
        data += data.left(data.indexOf('\n') + 1);
      },
      false, false);
  check(u"archive path traversal"_s,
        [](Files& files) { files[u"third-party-sources/../outside"_s] = "source"; });
  check(u"absolute archive path"_s,
        [](Files& files) { files[u"/third-party-sources/outside"_s] = "source"; });
  check(u"unexpected root"_s, [](Files& files) { files[u"outside/file"_s] = "source"; });
  check(u"duplicate archive member"_s, [](Files&) {}, false, true, u"licenses/NOTICE"_s);
  check(u"symlink archive member"_s, [](Files&) {}, false, true, {}, u"licenses/link"_s, '2');
  check(u"hardlink archive member"_s, [](Files&) {}, false, true, {}, u"licenses/link"_s, '1');
  check(u"invalid inventory"_s,
        [](Files& files) { files[u"third-party-sources/source-packages.json"_s] = "[1]"; });
  check(u"missing descriptor checksums"_s, [](Files& files) {
    files[u"third-party-sources/ubuntu/library_1.dsc"_s] = "Source: library\nVersion: 1\n";
  });
  check(u"invalid descriptor hash"_s, [](Files& files) {
    auto& data = files[u"third-party-sources/ubuntu/library_1.dsc"_s];
    data[data.indexOf("\n ") + 2] = 'g';
  });
  check(
      u"signed source descriptor"_s,
      [](Files& files) {
        auto& data = files[u"third-party-sources/ubuntu/library_1.dsc"_s];
        data = "-----BEGIN PGP SIGNED MESSAGE-----\nHash: SHA256\n\n" + data +
               "\n-----BEGIN PGP SIGNATURE-----\nfixture\n-----END PGP SIGNATURE-----\n";
      },
      true);

  check(u"canonical duplicate member"_s, [](Files&) {}, false, true, {}, u"./licenses//NOTICE"_s);
  check(u"legacy sparse member"_s, [](Files&) {}, false, true, {}, u"licenses/sparse"_s, 'S');
  check(
      u"invalid PAX length"_s, [](Files&) {}, false, true, {}, u"PaxHeaders/broken"_s, 'x',
      "999 path=licenses/NOTICE\n");
  check(
      u"nondecimal PAX length"_s, [](Files&) {}, false, true, {}, u"PaxHeaders/broken"_s, 'x',
      "invalid path=licenses/NOTICE\n");
  auto badTerminator = paxRecord("path", "licenses/NOTICE");
  badTerminator[badTerminator.size() - 1] = 'x';
  check(
      u"invalid PAX terminator"_s, [](Files&) {}, false, true, {}, u"PaxHeaders/broken"_s, 'x',
      badTerminator);
  check(
      u"empty PAX key"_s, [](Files&) {}, false, true, {}, u"PaxHeaders/broken"_s, 'x',
      paxRecord({}, "licenses/NOTICE"));
  check(
      u"PAX traversal"_s, [](Files&) {}, false, true, {}, {}, '0', {},
      paxRecord("path", "../outside"));
  check(
      u"absolute PAX path"_s, [](Files&) {}, false, true, {}, {}, '0', {},
      paxRecord("path", "/outside"));
  check(
      u"PAX size overflow"_s, [](Files&) {}, false, true, {}, {}, '0', {},
      paxRecord("size", "9223372036854775808"));
  check(
      u"negative PAX size"_s, [](Files&) {}, false, true, {}, {}, '0', {}, paxRecord("size", "-1"));
  check(
      u"SCHILY alternate file type"_s, [](Files&) {}, false, true, {}, {}, '0', {},
      paxRecord("SCHILY.filetype", "sparse"));
  check(
      u"SCHILY alternate size"_s, [](Files&) {}, false, true, {}, {}, '0', {},
      paxRecord("SCHILY.realsize", "1"));
  check(
      u"valid PAX path"_s, [](Files&) {}, true, true, {}, {}, '0', {},
      paxRecord("path", "licenses/NOTICE"));

  auto sparseFiles = fixture();
  manifest(sparseFiles);
  const auto sparseMetadata =
      paxRecord("GNU.sparse.major", "1") + paxRecord("GNU.sparse.minor", "0") +
      paxRecord("GNU.sparse.name", "licenses/MOVED") +
      paxRecord("GNU.sparse.realsize",
                QByteArray::number(sparseFiles[u"licenses/NOTICE"_s].size()));
  archive(path, sparseFiles, {}, {}, '0', {}, sparseMetadata, true);
  bool sparseRejected = false;
  try {
    verifySources(path, u"0.1.0-beta.5"_s);
  } catch (const std::exception&) {
    sparseRejected = true;
  }
  require(sparseRejected, u"GNU sparse metadata bypasses source companion verification."_s);
  ++count;
  const auto tar = QStandardPaths::findExecutable(u"tar"_s);
  if (!tar.isEmpty() && run(tar, {u"--version"_s}).contains("GNU tar")) {
    const auto names = run(tar, {u"--force-local"_s, u"--list"_s, u"--file"_s, path});
    require(names.split('\n').contains("licenses/MOVED") &&
                !names.split('\n').contains("licenses/NOTICE"),
            u"The GNU sparse fixture does not exercise the alternate archive path."_s);
    ++count;
  }

  QTextStream(stdout) << "Passed " << count << " source companion tests.\n";
  return 0;
}

}  // namespace release
