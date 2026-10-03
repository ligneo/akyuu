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

#include "sources.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <algorithm>
#include <stdexcept>

#include "archive.hpp"
#include "common.hpp"

using namespace Qt::StringLiterals;

namespace release {
namespace {

struct SourceFile {
  QString name;
  qint64 size;
  QByteArray digest;
};

struct Descriptor {
  QString package;
  QString version;
  QList<SourceFile> files;
};

bool regular(const ArchiveEntry& entry) {
  return entry.type == '0' || entry.type == '\0';
}

bool basename(const QString& name) {
  return !name.isEmpty() && name != u"." && name != u".." && !name.contains(u'/') &&
         !name.contains(u'\\') && !name.contains(QRegularExpression(u"[\\s\\x00]"_s));
}

void relativePath(const QString& name) {
  const auto parts = name.split(u'/');
  require(!name.isEmpty() && !name.startsWith(u'/') && !name.contains(u'\\') &&
              !name.contains(u'\0') && !parts.contains(u".."_s) && !parts.contains(u"."_s),
          u"Invalid archive path: %1"_s.arg(name));
}

QString field(const QString& text, const QString& key, const QString& name) {
  const QRegularExpression pattern(u"^%1: ([^\\r\\n]+)\\r?$"_s.arg(key),
                                   QRegularExpression::MultilineOption);
  auto matches = pattern.globalMatch(text);
  require(matches.hasNext(), u"Missing %1 in source descriptor: %2"_s.arg(key, name));
  const auto value = matches.next().captured(1).trimmed();
  require(!value.isEmpty() && !matches.hasNext(),
          u"Invalid %1 in source descriptor: %2"_s.arg(key, name));
  return value;
}

Descriptor descriptor(const QByteArray& data, const QString& name) {
  const auto text = QString::fromUtf8(data);
  Descriptor result{field(text, u"Source"_s, name), field(text, u"Version"_s, name), {}};
  const auto lines = text.split(u'\n');
  QSet<QString> names;
  bool found = false;
  bool reading = false;
  const QRegularExpression digestPattern(u"^[0-9a-f]{64}$"_s);
  const QRegularExpression sizePattern(u"^[0-9]+$"_s);
  const QRegularExpression whitespace(u"\\s+"_s);
  for (auto line : lines) {
    if (line.endsWith(u'\r')) line.chop(1);
    if (line == u"Checksums-Sha256:") {
      require(!found, u"Duplicate descriptor checksum field: %1"_s.arg(name));
      found = true;
      reading = true;
      continue;
    }
    if (!reading) continue;
    if (!line.startsWith(u' ') && !line.startsWith(u'\t')) {
      reading = false;
      continue;
    }
    const auto parts = line.trimmed().split(whitespace, Qt::SkipEmptyParts);
    require(parts.size() == 3, u"Invalid descriptor checksum entry: %1"_s.arg(name));
    require(digestPattern.match(parts[0]).hasMatch() && sizePattern.match(parts[1]).hasMatch(),
            u"Invalid descriptor checksum: %1"_s.arg(name));
    bool validSize = false;
    const auto size = parts[1].toLongLong(&validSize);
    require(validSize && size >= 0 && basename(parts[2]) && !names.contains(parts[2]),
            u"Invalid source filename or size: %1"_s.arg(parts[2]));
    names.insert(parts[2]);
    result.files.append({parts[2], size, parts[0].toLatin1()});
  }
  require(found && !result.files.isEmpty(), u"Missing descriptor checksums: %1"_s.arg(name));
  return result;
}

void copyFile(const QString& source, const QString& destination) {
  require(QFileInfo(source).isFile() && !QFileInfo(source).isSymLink(),
          u"Expected a source file: %1"_s.arg(source));
  require(QDir().mkpath(QFileInfo(destination).absolutePath()),
          u"Cannot create directory for %1"_s.arg(destination));
  if (QFileInfo::exists(destination)) {
    require(QFile::remove(destination), u"Cannot replace %1"_s.arg(destination));
  }
  require(QFile::copy(source, destination), u"Cannot copy %1 to %2"_s.arg(source, destination));
}

QStringList files(const QString& directory) {
  QStringList result;
  QDirIterator iterator(directory, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
  while (iterator.hasNext()) {
    iterator.next();
    if (!iterator.fileInfo().isSymLink()) result.append(iterator.filePath());
  }
  std::sort(result.begin(), result.end());
  return result;
}

QJsonObject inventory(const QByteArray& data, const QString& name) {
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(data, &error);
  require(error.error == QJsonParseError::NoError && document.isObject(),
          u"Invalid source inventory: %1"_s.arg(name));
  const auto result = document.object();
  require(!result.isEmpty(), u"Empty source inventory: %1"_s.arg(name));
  const QRegularExpression packagePattern(u"^[a-z0-9][a-z0-9+.-]*$"_s);
  for (auto it = result.begin(); it != result.end(); ++it) {
    const auto package = it.value().toObject();
    require(packagePattern.match(it.key()).hasMatch() && it.value().isObject() &&
                package.value(u"version"_s).isString() &&
                !package.value(u"version"_s).toString().isEmpty() &&
                package.value(u"binary"_s).isString() &&
                !package.value(u"binary"_s).toString().isEmpty(),
            u"Invalid source inventory entry: %1"_s.arg(it.key()));
  }
  return result;
}

QString packageOwner(const QString& origin) {
  QStringList paths{origin, QFileInfo(origin).canonicalFilePath()};
  if (origin.startsWith(u"/usr/")) paths.append(origin.mid(4));
  paths.removeDuplicates();
  for (const auto& path : paths) {
    if (path.isEmpty()) continue;
    try {
      const auto output = QString::fromUtf8(run(u"dpkg-query"_s, {u"-S"_s, path}));
      const auto first = output.section(u'\n', 0, 0);
      const auto position = first.indexOf(u": "_s);
      if (position > 0) return first.left(position);
    } catch (const std::runtime_error&) {
      // A merged-/usr alias may not be the path registered by dpkg.
    }
  }
  throw std::runtime_error(u"No package owner for %1"_s.arg(origin).toStdString());
}

QString libraryOrigin(const QString& name, const QMap<QString, QString>& libraries) {
  if (libraries.contains(name)) return libraries.value(name);
  QByteArray output;
  try {
    output = run(u"dpkg-query"_s, {u"-S"_s, u"*/%1"_s.arg(name)});
  } catch (const std::runtime_error&) {
    throw std::runtime_error(u"No system origin for bundled library: %1"_s.arg(name).toStdString());
  }
  for (const auto& line : QString::fromUtf8(output).split(u'\n')) {
    const auto position = line.indexOf(u": "_s);
    if (position < 0) continue;
    const auto path = line.mid(position + 2);
    if (QFileInfo(path).isFile() && QFileInfo(path).fileName() == name) return path;
  }
  throw std::runtime_error(u"No system origin for bundled library: %1"_s.arg(name).toStdString());
}

const ArchiveEntry& archiveFile(const TarArchive& archive, const QString& name) {
  const auto found = archive.entries().constFind(name);
  require(found != archive.entries().cend() && regular(found.value()),
          u"Expected an archive file: %1"_s.arg(name));
  return found.value();
}

}  // namespace

void collectSources(const QString& bundle, const QString& sdk) {
  const QDir bundleRoot(QFileInfo(bundle).absoluteFilePath());
  const QDir sdkRoot(QFileInfo(sdk).absoluteFilePath());
  const auto sources = sdkRoot.filePath(u"sources/third-party-sources"_s);
  const auto licenses = sdkRoot.filePath(u"licenses"_s);
  const auto ubuntu = sources + u"/ubuntu"_s;
  const auto cache = sdkRoot.filePath(u"cache/ubuntu-sources"_s);
  require(QDir(bundleRoot.filePath(u"usr"_s)).exists() && QDir(sources).exists() &&
              QDir(licenses).exists(),
          u"The bundle and SDK source/license directories must exist."_s);
  require(QDir().mkpath(cache), u"Cannot create source cache: %1"_s.arg(cache));
  if (QDir(ubuntu).exists()) {
    for (const auto& name : QDir(ubuntu).entryList(QDir::Files)) {
      copyFile(ubuntu + u'/' + name, cache + u'/' + name);
    }
    require(QDir(ubuntu).removeRecursively(), u"Cannot remove the old Ubuntu source inventory."_s);
  }
  require(QDir().mkpath(ubuntu), u"Cannot create Ubuntu source directory."_s);
  const auto ubuntuLicenses = licenses + u"/ubuntu"_s;
  if (QDir(ubuntuLicenses).exists()) {
    require(QDir(ubuntuLicenses).removeRecursively(), u"Cannot remove the old Ubuntu notices."_s);
  }

  QMap<QString, QString> libraries;
  for (const auto& line : QString::fromUtf8(run(u"ldconfig"_s, {u"-p"_s})).split(u'\n')) {
    const auto position = line.indexOf(u"=>"_s);
    if (position < 0) continue;
    const auto name = line.left(position).trimmed().section(u' ', 0, 0);
    if (!libraries.contains(name)) libraries.insert(name, line.mid(position + 2).trimmed());
  }

  QJsonObject packages;
  const QDir bundled(bundleRoot.filePath(u"usr"_s));
  for (const auto& path : files(bundled.path())) {
    const auto library = QFileInfo(path).fileName();
    if (!library.contains(u".so")) continue;
    const auto relative = bundled.relativeFilePath(path);
    if (relative.startsWith(u"plugins/") &&
        QFileInfo(sdkRoot.filePath(u"Qt/6.11.2/gcc_64/"_s + relative)).isFile()) {
      continue;
    }
    bool sdkLibrary = false;
    for (const auto& prefix :
         {u"libQt6", u"libstdc++", u"libgcc_s", u"libicu", u"libav", u"libsw"}) {
      if (library.startsWith(prefix)) sdkLibrary = true;
    }
    if (sdkLibrary) continue;
    const auto origin = libraryOrigin(library, libraries);
    const auto owner = packageOwner(origin);
    const auto output = QString::fromUtf8(
        run(u"dpkg-query"_s,
            {u"-W"_s, u"-f=${source:Package}\t${source:Version}\t${binary:Package}"_s, owner}));
    const auto fields = output.split(u'\t');
    require(fields.size() == 3 && !fields[0].isEmpty() && !fields[1].isEmpty(),
            u"Invalid package metadata for %1"_s.arg(owner));
    const auto source = fields[0];
    const auto packageVersion = fields[1];
    const auto binary = fields[2].trimmed();
    const auto noticeName = binary.section(u':', 0, 0);
    require(basename(source) && basename(noticeName), u"Invalid package metadata: %1"_s.arg(owner));
    if (packages.contains(source)) {
      require(packages.value(source).toObject().value(u"version"_s).toString() == packageVersion,
              u"Bundled binaries use different versions of source package %1"_s.arg(source));
    }
    packages.insert(source, QJsonObject{{u"version"_s, packageVersion}, {u"binary"_s, binary}});
    const auto notice = u"/usr/share/doc/%1/copyright"_s.arg(noticeName);
    copyFile(QFileInfo(notice).canonicalFilePath(),
             ubuntuLicenses + u'/' + noticeName + u"/copyright"_s);
  }
  require(!packages.isEmpty(), u"No Ubuntu source packages were collected."_s);

  for (auto it = packages.begin(); it != packages.end(); ++it) {
    const auto packageVersion = it.value().toObject().value(u"version"_s).toString();
    call(u"apt-get"_s, {u"source"_s, u"--download-only"_s, it.key() + u'=' + packageVersion},
         cache);
    QString descriptorPath;
    Descriptor source;
    for (const auto& name : QDir(cache).entryList({u"*.dsc"_s}, QDir::Files)) {
      const auto path = cache + u'/' + name;
      const auto text = QString::fromUtf8(readFile(path));
      const QRegularExpression packagePattern(
          u"^Source: %1\\r?$"_s.arg(QRegularExpression::escape(it.key())),
          QRegularExpression::MultilineOption);
      const QRegularExpression versionPattern(
          u"^Version: %1\\r?$"_s.arg(QRegularExpression::escape(packageVersion)),
          QRegularExpression::MultilineOption);
      if (!packagePattern.match(text).hasMatch() || !versionPattern.match(text).hasMatch())
        continue;
      require(descriptorPath.isEmpty(),
              u"Expected one source descriptor for %1=%2"_s.arg(it.key(), packageVersion));
      descriptorPath = path;
      source = descriptor(readFile(path), name);
    }
    require(!descriptorPath.isEmpty(),
            u"Expected one source descriptor for %1=%2"_s.arg(it.key(), packageVersion));
    copyFile(descriptorPath, ubuntu + u'/' + QFileInfo(descriptorPath).fileName());
    for (const auto& file : source.files) {
      const auto path = cache + u'/' + file.name;
      require(QFileInfo(path).isFile() && !QFileInfo(path).isSymLink() &&
                  QFileInfo(path).size() == file.size && sha256(path) == file.digest,
              u"Source checksum mismatch: %1"_s.arg(file.name));
      copyFile(path, ubuntu + u'/' + file.name);
    }
  }

  const auto packageData = QJsonDocument(packages).toJson(QJsonDocument::Indented);
  writeFile(sources + u"/source-packages.json"_s, packageData);
  writeFile(licenses + u"/source-packages.json"_s, packageData);
  QByteArray checksums;
  for (const auto& path : files(sources)) {
    if (QFileInfo(path).fileName() == u"SHA256SUMS") continue;
    checksums += sha256(path) + "  " + QDir(sources).relativeFilePath(path).toUtf8() + '\n';
  }
  writeFile(sources + u"/SHA256SUMS"_s, checksums);
  QTextStream(stdout) << "Collected sources and notices for " << packages.size()
                      << " bundled Ubuntu source packages.\n";
}

void verifySources(const QString& path, const QString& releaseVersion) {
  const TarArchive archive(path);
  for (auto it = archive.entries().cbegin(); it != archive.entries().cend(); ++it) {
    relativePath(it.key());
    const auto root = it.key().section(u'/', 0, 0);
    require(root == u"licenses" || root == u"third-party-sources",
            u"Unexpected archive root: %1"_s.arg(it.key()));
    require(it.key() != u"licenses/licenses/NOTICE" &&
                it.key() != u"licenses/licenses/source-packages.json",
            u"The license tree must not contain a nested copy."_s);
    require(regular(it.value()) || it.value().type == '5',
            u"Unsupported source companion entry: %1"_s.arg(it.key()));
  }
  const auto read = [&archive](const QString& name) {
    archiveFile(archive, name);
    return archive.read(name);
  };
  const auto sources = inventory(read(u"third-party-sources/source-packages.json"_s),
                                 u"third-party-sources/source-packages.json"_s);
  const auto notices =
      inventory(read(u"licenses/source-packages.json"_s), u"licenses/source-packages.json"_s);
  require(sources == notices, u"Source and license inventories do not match."_s);
  const auto companion = u"akyuu-%1-third-party-sources.tar"_s.arg(releaseVersion).toUtf8();
  for (const auto& name : {u"licenses/NOTICE"_s, u"third-party-sources/README"_s}) {
    require(read(name).contains(companion), u"The release version is missing from %1"_s.arg(name));
  }

  QSet<QString> listed;
  const QRegularExpression digestPattern(u"^[0-9a-f]{64}$"_s);
  for (const auto& line :
       QString::fromUtf8(read(u"third-party-sources/SHA256SUMS"_s)).split(u'\n')) {
    if (line.isEmpty()) continue;
    const auto position = line.indexOf(u"  "_s);
    require(position == 64, u"Invalid checksum entry: %1"_s.arg(line));
    const auto digest = line.left(position);
    const auto name = line.mid(position + 2);
    relativePath(name);
    require(digestPattern.match(digest).hasMatch() && !listed.contains(name),
            u"Invalid checksum entry: %1"_s.arg(name));
    listed.insert(name);
    const auto memberName = u"third-party-sources/"_s + name;
    const auto& entry = archiveFile(archive, memberName);
    require(entry.digest == digest.toLatin1(), u"Checksum mismatch: %1"_s.arg(name));
  }
  QSet<QString> payload;
  QSet<QString> actualUbuntu;
  for (auto it = archive.entries().cbegin(); it != archive.entries().cend(); ++it) {
    if (!regular(it.value())) continue;
    if (it.key().startsWith(u"third-party-sources/") &&
        it.key() != u"third-party-sources/SHA256SUMS") {
      payload.insert(it.key().mid(20));
    }
    if (it.key().startsWith(u"third-party-sources/ubuntu/")) actualUbuntu.insert(it.key());
  }
  require(listed == payload, u"The checksum manifest does not cover the exact source payload."_s);

  QSet<QString> descriptors;
  QSet<QString> expectedUbuntu;
  for (const auto& name : actualUbuntu) {
    if (!name.endsWith(u".dsc")) continue;
    const auto source = descriptor(read(name), name);
    require(sources.contains(source.package) && !descriptors.contains(source.package) &&
                sources.value(source.package).toObject().value(u"version"_s).toString() ==
                    source.version,
            u"Source descriptor does not match the inventory: %1"_s.arg(name));
    descriptors.insert(source.package);
    expectedUbuntu.insert(name);
    for (const auto& file : source.files) {
      const auto memberName = u"third-party-sources/ubuntu/"_s + file.name;
      expectedUbuntu.insert(memberName);
      const auto& entry = archiveFile(archive, memberName);
      require(entry.size == file.size && entry.digest == file.digest,
              u"Descriptor checksum mismatch: %1"_s.arg(file.name));
    }
  }
  QSet<QString> expectedPackages;
  for (auto it = sources.begin(); it != sources.end(); ++it) expectedPackages.insert(it.key());
  require(descriptors == expectedPackages,
          u"Source descriptors do not cover the exact inventory."_s);
  require(actualUbuntu == expectedUbuntu,
          u"The Ubuntu payload contains missing or unreferenced source files."_s);
  QTextStream(stdout) << "Verified " << sources.size() << " source packages and " << listed.size()
                      << " companion checksums.\n";
}

}  // namespace release
