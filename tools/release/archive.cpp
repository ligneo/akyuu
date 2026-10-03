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

#include "archive.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <limits>

#include "common.hpp"

namespace release {
namespace {
QByteArray field(const QByteArray& header, int offset, int size) {
  auto value = header.mid(offset, size);
  const auto zero = value.indexOf('\0');
  if (zero >= 0) value.truncate(zero);
  return value;
}
qint64 number(QByteArray value) {
  require(value.isEmpty() || !(static_cast<unsigned char>(value[0]) & 0x80),
          "Binary TAR numbers are unsupported.");
  value.replace('\0', ' ');
  value = value.trimmed();
  if (value.isEmpty()) return 0;
  for (const auto byte : value) require(byte >= '0' && byte <= '7', "Invalid TAR number.");
  bool ok = false;
  const auto result = value.toLongLong(&ok, 8);
  require(ok && result >= 0, "TAR integer overflow.");
  return result;
}
QMap<QByteArray, QByteArray> pax(const QByteArray& data) {
  QMap<QByteArray, QByteArray> result;
  qsizetype offset = 0;
  while (offset < data.size()) {
    const auto space = data.indexOf(' ', offset);
    require(space > offset, "Invalid PAX length.");
    bool ok = false;
    const auto length = data.mid(offset, space - offset).toLongLong(&ok);
    require(ok && length > space - offset + 2 && length <= data.size() - offset,
            "Invalid PAX record.");
    auto record = data.mid(space + 1, offset + length - space - 1);
    require(record.endsWith('\n'), "Invalid PAX terminator.");
    record.chop(1);
    const auto equal = record.indexOf('=');
    require(equal > 0, "Invalid PAX attribute.");
    const auto key = record.left(equal);
    require(!key.startsWith("GNU.sparse.") && key != "SCHILY.filetype" && key != "SCHILY.realsize",
            "Sparse TAR metadata is unsupported.");
    result.insert(key, record.mid(equal + 1));
    offset += length;
  }
  return result;
}
QString canonicalName(const QByteArray& encoded) {
  require(!encoded.contains('\0'), "Embedded NUL in TAR path.");
  const auto path = QString::fromUtf8(encoded);
  require(
      path.toUtf8() == encoded && !path.isEmpty() && !path.startsWith('/') && !path.contains('\\'),
      "Unsafe TAR path: " + path);
  QStringList parts;
  for (const auto& part : path.split('/')) {
    require(part != "..", "Unsafe TAR traversal: " + path);
    if (!part.isEmpty() && part != ".") parts << part;
  }
  require(!parts.isEmpty() && !parts.first().contains(':'), "Unsafe TAR path: " + path);
  return parts.join('/');
}
}  // namespace
TarArchive::TarArchive(const QString& path) : path_(path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Cannot open archive: " + path);
  QMap<QByteArray, QByteArray> global, pending;
  QByteArray longName, longLink;
  while (file.pos() < file.size()) {
    const auto header = file.read(512);
    require(header.size() == 512, "Truncated TAR header.");
    if (header == QByteArray(512, '\0')) {
      const auto second = file.read(512);
      require(second == QByteArray(512, '\0'), "Missing TAR end marker.");
      // Trailing TAR padding is allowed, trailing payload is not.
      while (!file.atEnd()) {
        const auto padding = file.read(65536);
        require(padding == QByteArray(padding.size(), '\0'), "Unexpected trailing archive data.");
      }
      return;
    }
    qint64 checksum = 0;
    for (int i = 0; i < 512; ++i)
      checksum += (i >= 148 && i < 156) ? 32 : static_cast<unsigned char>(header[i]);
    require(checksum == number(header.mid(148, 8)), "Invalid TAR header checksum.");
    auto size = number(header.mid(124, 12));
    const auto type = header[156] == '\0' ? '0' : header[156];
    require(type != 'S', "Sparse TAR entries are unsupported.");
    const auto offset = file.pos();
    require(size <= file.size() - offset && size <= std::numeric_limits<qint64>::max() - 511,
            "Truncated or oversized TAR payload.");
    if (type == 'x' || type == 'g' || type == 'L' || type == 'K') {
      require(size <= 1048576, "Oversized TAR metadata.");
      const auto data = file.read(size);
      require(data.size() == size, "Truncated TAR metadata.");
      if (type == 'g') global.insert(pax(data));
      if (type == 'x') pending.insert(pax(data));
      if (type == 'L') longName = data.split('\0').first();
      if (type == 'K') longLink = data.split('\0').first();
    } else {
      auto attributes = global;
      attributes.insert(pending);
      auto name = field(header, 0, 100);
      if (header.mid(257, 5) == "ustar" && !field(header, 345, 155).isEmpty())
        name = field(header, 345, 155) + '/' + name;
      if (!longName.isEmpty()) name = longName;
      if (attributes.contains("path")) name = attributes.value("path");
      auto link = field(header, 157, 100);
      if (!longLink.isEmpty()) link = longLink;
      if (attributes.contains("linkpath")) link = attributes.value("linkpath");
      if (attributes.contains("size")) {
        bool ok = false;
        size = attributes.value("size").toLongLong(&ok);
        require(ok && size >= 0 && size <= file.size() - offset &&
                    size <= std::numeric_limits<qint64>::max() - 511,
                "Invalid PAX size.");
      }
      const auto pathName = canonicalName(name);
      require(!entries_.contains(pathName), "Duplicate TAR path: " + pathName);
      QByteArray digest;
      if (type == '0') {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        qint64 remaining = size;
        while (remaining > 0) {
          const auto block = file.read(qMin<qint64>(remaining, 1048576));
          require(!block.isEmpty(), "Truncated TAR file.");
          hash.addData(block);
          remaining -= block.size();
        }
        digest = hash.result().toHex();
      }
      entries_.insert(pathName, {pathName, type, size, offset, QString::fromUtf8(link), digest});
      pending.clear();
      longName.clear();
      longLink.clear();
    }
    const auto padded = ((size + 511) / 512) * 512;
    require(padded <= file.size() - offset && file.seek(offset + padded), "Truncated TAR padding.");
  }
  require(false, "Missing TAR end marker.");
}
QByteArray TarArchive::read(const QString& name, qint64 limit) const {
  const auto found = entries_.constFind(name);
  require(found != entries_.cend() && found->type == '0', "Missing TAR file: " + name);
  require(found->size <= limit, "TAR metadata file exceeds its size limit: " + name);
  QFile file(path_);
  require(file.open(QIODevice::ReadOnly) && file.seek(found->offset),
          "Cannot read TAR entry: " + name);
  const auto data = file.read(found->size);
  require(data.size() == found->size, "Truncated TAR entry: " + name);
  return data;
}
}  // namespace release
