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

#include "common.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <filesystem>
#include <stdexcept>

namespace release {
void require(bool condition, const QString& message) {
  if (!condition) throw std::runtime_error(message.toStdString());
}
QByteArray readFile(const QString& path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Cannot read " + path + ": " + file.errorString());
  const auto data = file.readAll();
  require(file.error() == QFileDevice::NoError, "Reading failed: " + path);
  return data;
}
void writeFile(const QString& path, const QByteArray& data) {
  require(QDir().mkpath(QFileInfo(path).absolutePath()), "Cannot create parent directory: " + path);
  QSaveFile file(path);
  require(file.open(QIODevice::WriteOnly), "Cannot write " + path + ": " + file.errorString());
  require(file.write(data) == data.size() && file.commit(), "Writing failed: " + path);
}
static QByteArray execute(const QString& program, const QStringList& arguments,
                          const QString& directory, int timeout, bool forward) {
  QProcess process;
  if (!directory.isEmpty()) process.setWorkingDirectory(directory);
  process.setProcessChannelMode(forward ? QProcess::ForwardedChannels : QProcess::SeparateChannels);
  process.start(program, arguments);
  require(process.waitForStarted(), "Cannot start " + program + ": " + process.errorString());
  if (!process.waitForFinished(timeout)) {
    process.kill();
    process.waitForFinished();
    require(false, "Process timed out: " + program);
  }
  const auto output = process.readAllStandardOutput();
  const auto errors = process.readAllStandardError();
  require(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
          QString("%1 failed (%2): %3")
              .arg(program)
              .arg(process.exitCode())
              .arg(QString::fromUtf8(errors)));
  return output;
}
QByteArray run(const QString& program, const QStringList& arguments, const QString& directory,
               int timeout) {
  return execute(program, arguments, directory, timeout, false);
}
void call(const QString& program, const QStringList& arguments, const QString& directory,
          int timeout) {
  execute(program, arguments, directory, timeout, true);
}
void validateVersion(const QString& value) {
  const auto expression = QRegularExpression(
      "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*)"
      ")?$");
  const auto match = expression.match(value);
  require(match.hasMatch(), "Expected MAJOR.MINOR.PATCH[-PRERELEASE].");
  for (const auto& identifier : match.captured(4).split('.'))
    require(!QRegularExpression("^0[0-9]+$").match(identifier).hasMatch(),
            "Numeric prerelease identifiers must not have leading zeroes.");
}
QString version(const QString& root) {
  const auto text = QString::fromUtf8(readFile(root + "/src/akyuu/config.h"));
  QStringList parts;
  for (const auto& name : {"MAJOR", "MINOR", "PATCH"}) {
    const auto match = QRegularExpression(QString("^#define AKYUU_VERSION_%1 +([0-9]+)$").arg(name),
                                          QRegularExpression::MultilineOption)
                           .match(text);
    require(match.hasMatch(), "Missing version component: " + QString(name));
    parts << match.captured(1);
  }
  const auto match = QRegularExpression("^#define AKYUU_VERSION_PRE +\"([^\"]*)\"$",
                                        QRegularExpression::MultilineOption)
                         .match(text);
  require(match.hasMatch(), "Missing prerelease component.");
  const auto result =
      parts.join('.') + (match.captured(1).isEmpty() ? QString() : "-" + match.captured(1));
  validateVersion(result);
  return result;
}
QByteArray sha256(const QString& path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Cannot hash " + path);
  QCryptographicHash hash(QCryptographicHash::Sha256);
  require(hash.addData(&file), "Cannot hash " + path);
  return hash.result().toHex();
}
void copyTree(const QString& source, const QString& destination) {
  const QFileInfo info(source);
  require(info.exists() || info.isSymLink(), "Missing copy source: " + source);
  if (info.isSymLink()) {
    require(QDir().mkpath(QFileInfo(destination).absolutePath()),
            "Cannot create copy destination.");
    QFile::remove(destination);
#ifdef Q_OS_UNIX
    std::error_code error;
    const auto target = std::filesystem::read_symlink(QFile::encodeName(source).constData(), error);
    require(!error, "Cannot read symbolic link: " + source);
    std::filesystem::create_symlink(target, QFile::encodeName(destination).constData(), error);
    require(!error, "Cannot copy symbolic link: " + source);
#else
    require(false, "Symbolic links are not supported in the Windows payload: " + source);
#endif
  } else if (info.isDir()) {
    require(QDir().mkpath(destination), "Cannot create " + destination);
    for (const auto& child : QDir(source).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
                                                        QDir::Hidden | QDir::System))
      copyTree(child.absoluteFilePath(), destination + '/' + child.fileName());
    require(QFile::setPermissions(destination, info.permissions()),
            "Cannot set directory permissions: " + destination);
  } else {
    require(info.isFile(), "Unsupported copy source: " + source);
    require(QDir().mkpath(QFileInfo(destination).absolutePath()), "Cannot create copy parent.");
    QFile::remove(destination);
    require(QFile::copy(source, destination), "Cannot copy " + source);
    require(QFile::setPermissions(destination, info.permissions()),
            "Cannot set file permissions: " + destination);
  }
}
}  // namespace release
