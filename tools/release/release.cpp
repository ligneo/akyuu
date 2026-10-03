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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <functional>

#include "archive.hpp"
#include "common.hpp"
#include "sources.hpp"

namespace release {
namespace {
void appendBytes(QFile& file, const QByteArray& data) {
  require(file.write(data) == data.size(), "Cannot write source archive.");
}
void octal(QByteArray& header, int offset, int length, qint64 value) {
  const auto digits = QByteArray::number(value, 8);
  require(value >= 0 && digits.size() < length, "TAR number overflow.");
  header.replace(offset, length, digits.rightJustified(length - 1, '0') + '\0');
}
void tarHeader(QFile& archive, const QByteArray& name, char type, qint64 size,
               const QByteArray& mode, const QByteArray& time, const QByteArray& link = {}) {
  // GNU long-name records preserve paths without extracting an untrusted tree.
  const auto longField = [&](const QByteArray& data, char kind) {
    if (data.size() <= 100) return;
    tarHeader(archive, "././@LongLink", kind, data.size() + 1, QByteArray("0000644\0", 8),
              QByteArray("00000000000\0", 12));
    appendBytes(archive, data + '\0');
    appendBytes(archive, QByteArray((512 - (data.size() + 1) % 512) % 512, '\0'));
  };
  longField(name, 'L');
  longField(link, 'K');
  QByteArray header(512, '\0');
  header.replace(0, qMin<qsizetype>(100, name.size()), name.left(100));
  header.replace(100, 8, mode);
  octal(header, 108, 8, 0);
  octal(header, 116, 8, 0);
  octal(header, 124, 12, size);
  header.replace(136, 12, time);
  header.replace(148, 8, QByteArray(8, ' '));
  header[156] = type;
  header.replace(157, qMin<qsizetype>(100, link.size()), link.left(100));
  header.replace(257, 8, QByteArray("ustar  \0", 8));
  qint64 checksum = 0;
  for (const auto byte : header) checksum += static_cast<unsigned char>(byte);
  header.replace(148, 8, QByteArray::number(checksum, 8).rightJustified(6, '0') + '\0' + ' ');
  appendBytes(archive, header);
}
QByteArray git(const QString& root, const QStringList& arguments) {
  return run("git", QStringList{"-c", "safe.directory=" + root, "-C", root} + arguments);
}
}  // namespace
void sourceBundle(const QString& root, const QString& output) {
  const auto current = version(root);
  require(QDir().mkpath(output), "Cannot create source output directory.");
  QTemporaryDir temporary;
  require(temporary.isValid(), "Cannot create archive staging directory.");
  QFile archive(temporary.path() + "/source.tar");
  require(archive.open(QIODevice::WriteOnly), "Cannot create source archive.");
  int count = 0;
  QSet<QString> names;
  std::function<void(QString, QString)> append = [&](const QString& repo, const QString& prefix) {
    require(git(repo, {"status", "--porcelain", "--untracked-files=normal"}).trimmed().isEmpty(),
            "Source tree must be clean: " + repo);
    const auto fileName = temporary.path() + QString("/tree-%1.tar").arg(count++);
    git(repo, {"archive", "--format=tar", "--output=" + fileName, "HEAD"});
    TarArchive source(fileName);
    QFile input(fileName);
    require(input.open(QIODevice::ReadOnly), "Cannot read Git archive.");
    for (const auto& entry : source.entries()) {
      const auto name = prefix + '/' + entry.name + (entry.type == '5' ? "/" : "");
      require(!names.contains(name), "Duplicate source path: " + name);
      names.insert(name);
      require(entry.type == '0' || entry.type == '2' || entry.type == '5',
              "Unsupported Git archive member: " + entry.name);
      require(input.seek(entry.offset - 512), "Cannot seek Git archive header.");
      const auto original = input.read(512);
      require(original.size() == 512, "Cannot read Git archive header.");
      tarHeader(archive, name.toUtf8(), entry.type, entry.size, original.mid(100, 8),
                original.mid(136, 12), entry.link.toUtf8());
      qint64 remaining = entry.size;
      while (remaining > 0) {
        const auto block = input.read(qMin<qint64>(remaining, 1048576));
        require(!block.isEmpty(), "Truncated source file.");
        appendBytes(archive, block);
        remaining -= block.size();
      }
      appendBytes(archive, QByteArray((512 - entry.size % 512) % 512, '\0'));
    }
    for (const auto& record : git(repo, {"ls-tree", "-rz", "HEAD"}).split('\0')) {
      if (record.isEmpty()) continue;
      const auto tab = record.indexOf('\t');
      require(tab > 0, "Invalid Git tree record.");
      const auto metadata = record.left(tab).split(' ');
      require(metadata.size() == 3, "Invalid Git tree metadata.");
      if (metadata[0] != "160000") continue;
      const auto relative = QString::fromUtf8(record.mid(tab + 1));
      const auto child = repo + '/' + relative;
      require(git(child, {"rev-parse", "HEAD"}).trimmed() == metadata[2],
              "Submodule does not match its recorded commit: " + child);
      append(child, prefix + '/' + relative);
    }
  };
  append(root, "akyuu-" + current);
  appendBytes(archive, QByteArray(1024, '\0'));
  archive.close();
  // xz is a build dependency, independent of the installed application's runtime.
  const auto destination = QDir(output).absoluteFilePath("akyuu-" + current + "-source.tar.xz");
  const auto staged = temporary.path() + "/source.tar.xz";
  QProcess compressor;
  compressor.setStandardOutputFile(staged);
  compressor.setProcessChannelMode(QProcess::ForwardedErrorChannel);
  compressor.start("xz", {"--threads=1", "--stdout", archive.fileName()});
  require(compressor.waitForStarted() && compressor.waitForFinished(300000) &&
              compressor.exitStatus() == QProcess::NormalExit && compressor.exitCode() == 0,
          "Source archive compression failed.");
  writeFile(destination, readFile(staged));
  QTextStream(stdout) << destination << '\n';
}
void bumpVersion(const QString& root, const QString& next) {
  const auto match = QRegularExpression(
                         "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:"
                         "\\.[0-9A-Za-z-]+)*))?$")
                         .match(next);
  require(match.hasMatch(), "Expected MAJOR.MINOR.PATCH[-PRERELEASE].");
  validateVersion(next);
  require(git(root, {"status", "--porcelain", "--untracked-files=no"}).trimmed().isEmpty(),
          "The working tree has uncommitted changes.");
  require(
      !QString::fromUtf8(git(root, {"tag", "--list", "v" + next})).split('\n').contains("v" + next),
      "Tag already exists: v" + next);
  const auto config = root + "/src/akyuu/config.h";
  auto text = QString::fromUtf8(readFile(config));
  QStringList components{"MAJOR", "MINOR", "PATCH", "PRE"};
  for (int i = 0; i < components.size(); ++i) {
    const auto replacement = i == 3 ? "\"" + match.captured(4) + "\"" : match.captured(i + 1);
    const auto expression =
        QRegularExpression("^#define AKYUU_VERSION_" + components[i] + " +[^\\n]+$",
                           QRegularExpression::MultilineOption);
    require(expression.match(text).hasMatch(), "Missing version macro.");
    text.replace(expression,
                 "#define AKYUU_VERSION_" + components[i] + (i == 3 ? "   " : " ") + replacement);
  }
  writeFile(config, text.toUtf8());
  if (!git(root, {"diff", "--", "src/akyuu/config.h"}).isEmpty())
    git(root, {"commit", "-q", "-m", "Bump version to " + next, "--", "src/akyuu/config.h"});
  git(root, {"tag", "-a", "v" + next, "-m", "Akyuu " + next});
  QTextStream(stdout) << "Created tag v" << next << ". Nothing was pushed.\n";
}
void createReleaseDraft(const QString& root, const QString& tag, const QString& packages) {
  const auto current = version(root);
  require(tag == "v" + current, "Tag does not match the source version.");
  QString archVersion = current;
  archVersion.remove('-');
  const QStringList assets{"akyuu-" + current + "-x86_64.AppImage",
                           "akyuu-" + current + "-x86_64.deb",
                           "akyuu-" + current + "-x86_64.rpm",
                           "akyuu-" + current + "-x86_64-setup.exe",
                           "akyuu-" + archVersion + "-1-x86_64.pkg.tar.zst",
                           "akyuu-" + current + "-source.tar.xz",
                           "akyuu-" + current + "-third-party-sources.tar",
                           "PKGBUILD"};
  for (const auto& asset : assets) {
    const QFileInfo info(packages + '/' + asset);
    require(info.isFile() && !info.isSymLink() && info.size() > 0,
            "Missing or unsafe release asset: " + asset);
  }
  const auto tags = run("gh", {"api", "--paginate", "repos/ligneo/akyuu/releases?per_page=100",
                               "--jq", ".[].tag_name"});
  require(!QString::fromUtf8(tags).split('\n').contains(tag),
          "A release already exists; published files are immutable.");
  verifySources(packages + "/akyuu-" + current + "-third-party-sources.tar", current);
  QByteArray manifest;
  for (const auto& asset : assets)
    manifest += sha256(packages + '/' + asset) + "  " + asset.toUtf8() + '\n';
  writeFile(packages + "/SHA256SUMS", manifest);
  QTemporaryDir temporary;
  require(temporary.isValid(), "Cannot create draft staging directory.");
  writeFile(
      temporary.path() + "/notes.md",
      QByteArray("See the [changelog](https://github.com/ligneo/akyuu/wiki/Changelog) and "
                 "[installation guide](https://github.com/ligneo/akyuu/wiki/How-to-Compile).\n\n"
                 "Packages: Linux x86_64 AppImage, Arch, Debian/Ubuntu, Fedora/openSUSE, and "
                 "Windows x64 installer. macOS is deferred. Native Linux runtime tests cover "
                 "Ubuntu 24.04, Debian 13, Fedora 44 and openSUSE Leap 16.0; desktop and player "
                 "integration testing remains separate. The Windows installer is unsigned.\n\n"
                 "Full application and corresponding third-party sources accompany the binaries. "
                 "Downloads are checked against SHA256SUMS; installation remains with the user or "
                 "package manager.\n"));
  QStringList arguments{"release", "create", tag};
  for (const auto& asset : assets) arguments << packages + '/' + asset;
  arguments << packages + "/SHA256SUMS" << "--repo" << "ligneo/akyuu" << "--verify-tag"
            << "--draft" << "--title" << "Akyuu " + current << "--notes-file"
            << temporary.path() + "/notes.md";
  if (current.contains('-')) arguments << "--prerelease";
  call("gh", arguments);
  const auto download = temporary.path() + "/download";
  call("gh", {"release", "download", tag, "--repo", "ligneo/akyuu", "--dir", download});
  require(readFile(download + "/SHA256SUMS") == manifest, "Uploaded checksum manifest differs.");
  for (const auto& asset : assets)
    require(sha256(download + '/' + asset) == sha256(packages + '/' + asset),
            "Uploaded asset differs: " + asset);
  QTextStream(stdout)
      << "Draft created and uploaded files verified. Publication remains a maintainer action.\n";
}
}  // namespace release
