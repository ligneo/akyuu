/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
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

#include "feed_filter_util.hpp"

#include <QByteArray>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <map>

#include "akyuu/version.hpp"

namespace track::util {

namespace {

using namespace Qt::StringLiterals;

const std::map<FilterAction, QString> kActions{
    {FilterAction::Discard, u"discard"_s},
    {FilterAction::Select, u"select"_s},
    {FilterAction::Prefer, u"prefer"_s},
};

const std::map<FilterElement, QString> kElements{
    {FilterElement::MetaId, u"meta_id"_s},
    {FilterElement::MetaStatus, u"meta_status"_s},
    {FilterElement::MetaType, u"meta_type"_s},
    {FilterElement::MetaEpisodes, u"meta_episodes"_s},
    {FilterElement::MetaDateStart, u"meta_date_start"_s},
    {FilterElement::MetaDateEnd, u"meta_date_end"_s},
    // v1 named this one after the field it used to be, and its filters still say so.
    {FilterElement::UserNotes, u"user_tags"_s},
    {FilterElement::UserStatus, u"user_status"_s},
    {FilterElement::LocalEpisodeAvailable, u"local_episode_available"_s},
    {FilterElement::EpisodeTitle, u"episode_title"_s},
    {FilterElement::EpisodeNumber, u"episode_number"_s},
    {FilterElement::EpisodeVersion, u"episode_version"_s},
    {FilterElement::EpisodeGroup, u"episode_group"_s},
    {FilterElement::EpisodeVideoResolution, u"episode_video_resolution"_s},
    {FilterElement::EpisodeVideoType, u"episode_video_type"_s},
    {FilterElement::FileTitle, u"file_title"_s},
    {FilterElement::FileCategory, u"file_category"_s},
    {FilterElement::FileDescription, u"file_description"_s},
    {FilterElement::FileLink, u"file_link"_s},
    {FilterElement::FileSize, u"file_size"_s},
};

const std::map<FilterMatch, QString> kMatches{
    {FilterMatch::All, u"all"_s},
    {FilterMatch::Any, u"any"_s},
};

const std::map<FilterOperator, QString> kOperators{
    {FilterOperator::Equals, u"equals"_s},
    {FilterOperator::NotEquals, u"notequals"_s},
    {FilterOperator::IsGreaterThan, u"gt"_s},
    {FilterOperator::IsGreaterThanOrEqualTo, u"ge"_s},
    {FilterOperator::IsLessThan, u"lt"_s},
    {FilterOperator::IsLessThanOrEqualTo, u"le"_s},
    {FilterOperator::BeginsWith, u"beginswith"_s},
    {FilterOperator::EndsWith, u"endswith"_s},
    {FilterOperator::Contains, u"contains"_s},
    {FilterOperator::NotContains, u"notcontains"_s},
};

const std::map<FilterOption, QString> kOptions{
    {FilterOption::Default, u"default"_s},
    {FilterOption::Deactivate, u"deactivate"_s},
    {FilterOption::Hide, u"hide"_s},
};

template <typename T>
QString toShortcode(const std::map<T, QString>& map, const T value) {
  const auto it = map.find(value);
  return it != map.end() ? it->second : QString{};
}

template <typename T>
std::optional<T> fromShortcode(const std::map<T, QString>& map, const QString& shortcode) {
  for (const auto& [value, text] : map) {
    if (text.compare(shortcode, Qt::CaseInsensitive) == 0) return value;
  }
  return std::nullopt;
}

}  // namespace

// v1's `StringCoder`: "TAI", a version byte, then little-endian 16-bit sizes around the metadata
// and the deflated payload. The whole buffer is Base64'd.
namespace {

constexpr auto kMagic = "TAI";
constexpr char kCoderVersion = 0x01;

void appendSize(QByteArray& buffer, const quint16 value) {
  buffer.append(static_cast<char>(value & 0xFF));
  buffer.append(static_cast<char>((value >> 8) & 0xFF));
}

quint16 readSize(const QByteArray& buffer, const int pos) {
  if (pos + 2 > buffer.size()) return 0;
  return static_cast<quint8>(buffer.at(pos)) | (static_cast<quint8>(buffer.at(pos + 1)) << 8);
}

// `qCompress` is zlib's `compress()` with a four-byte big-endian length in front, which is exactly
// what v1 writes without that prefix.
QByteArray deflate(const QByteArray& data) {
  return qCompress(data).mid(4);
}

QByteArray inflate(const QByteArray& data, const quint16 size) {
  QByteArray prefixed;
  prefixed.append(static_cast<char>((size >> 24) & 0xFF));
  prefixed.append(static_cast<char>((size >> 16) & 0xFF));
  prefixed.append(static_cast<char>((size >> 8) & 0xFF));
  prefixed.append(static_cast<char>(size & 0xFF));
  prefixed.append(data);
  return qUncompress(prefixed);
}

}  // namespace

QString filtersToXml(const std::vector<Filter>& filters) {
  QString output;
  QXmlStreamWriter writer(&output);

  // v1 writes the items straight into the document, with no element around them.
  writer.setAutoFormatting(true);
  writer.setAutoFormattingIndent(1);

  for (const auto& filter : filters) {
    writer.writeStartElement(u"item"_s);
    writer.writeAttribute(u"action"_s, shortcode(filter.action));
    writer.writeAttribute(u"match"_s, shortcode(filter.match));
    writer.writeAttribute(u"option"_s, shortcode(filter.option));
    writer.writeAttribute(u"enabled"_s, filter.enabled ? u"true"_s : u"false"_s);
    writer.writeAttribute(u"name"_s, QString::fromStdString(filter.name));

    for (const auto id : filter.anime_ids) {
      writer.writeStartElement(u"anime"_s);
      writer.writeAttribute(u"id"_s, QString::number(id));
      writer.writeEndElement();
    }

    for (const auto& condition : filter.conditions) {
      writer.writeStartElement(u"condition"_s);
      writer.writeAttribute(u"element"_s, shortcode(condition.element));
      writer.writeAttribute(u"operator"_s, shortcode(condition.op));
      writer.writeAttribute(u"value"_s, QString::fromStdString(condition.value));
      writer.writeEndElement();
    }

    writer.writeEndElement();
  }

  return output;
}

std::optional<std::vector<Filter>> filtersFromXml(const QString& xml) {
  auto text = xml.trimmed();

  // Several items at the top level are not a well-formed document, so they are given a root here.
  // A declaration, if v1 wrote one, has to stay in front of it.
  if (text.startsWith(u"<?xml"_s)) {
    const auto end = text.indexOf(u"?>"_s);
    if (end < 0) return std::nullopt;
    text.remove(0, end + 2);
  }
  text = u"<filters>%1</filters>"_s.arg(text);

  std::vector<Filter> filters;
  QXmlStreamReader reader(text);

  while (!reader.atEnd()) {
    if (reader.readNext() != QXmlStreamReader::StartElement) continue;
    if (reader.name() != u"item"_s) continue;

    const auto attributes = reader.attributes();

    Filter filter;
    filter.name = attributes.value(u"name"_s).toString().toStdString();
    filter.enabled =
        attributes.value(u"enabled"_s).toString().compare(u"false"_s, Qt::CaseInsensitive) != 0 &&
        attributes.value(u"enabled"_s).toString() != u"0"_s;
    filter.action =
        filterAction(attributes.value(u"action"_s).toString()).value_or(FilterAction::Discard);
    filter.match = filterMatch(attributes.value(u"match"_s).toString()).value_or(FilterMatch::All);
    filter.option =
        filterOption(attributes.value(u"option"_s).toString()).value_or(FilterOption::Default);

    while (!(reader.readNext() == QXmlStreamReader::EndElement && reader.name() == u"item"_s)) {
      if (reader.atEnd() || reader.hasError()) break;
      if (reader.tokenType() != QXmlStreamReader::StartElement) continue;

      const auto child = reader.attributes();

      if (reader.name() == u"anime"_s) {
        filter.anime_ids.push_back(child.value(u"id"_s).toInt());
      } else if (reader.name() == u"condition"_s) {
        const auto element = filterElement(child.value(u"element"_s).toString());
        const auto op = filterOperator(child.value(u"operator"_s).toString());
        // A condition Akyuu cannot read would change what the filter does, so it is left out
        // rather than guessed.
        if (!element || !op) continue;
        filter.conditions.push_back(
            {*element, *op, child.value(u"value"_s).toString().toStdString()});
      }
    }

    filters.push_back(std::move(filter));
  }

  if (reader.hasError()) return std::nullopt;

  return filters;
}

QString encodeFilters(const std::vector<Filter>& filters) {
  const auto data = filtersToXml(filters).toUtf8();

  if (data.isEmpty()) return {};

  const auto metadata = QString::fromStdString(akyuu::version().to_string()).toUtf8();
  const auto compressed = deflate(data);

  QByteArray buffer;
  buffer.append(kMagic);
  buffer.append(kCoderVersion);
  appendSize(buffer, static_cast<quint16>(metadata.size()));
  buffer.append(metadata);
  appendSize(buffer, static_cast<quint16>(compressed.size()));
  appendSize(buffer, static_cast<quint16>(data.size()));
  buffer.append(compressed);

  return QString::fromLatin1(buffer.toBase64());
}

std::optional<std::vector<Filter>> decodeFilters(const QString& input) {
  const auto buffer = QByteArray::fromBase64(input.trimmed().toLatin1());

  // magic + version + two sizes around the metadata and the payload
  if (buffer.size() < 3 + 1 + 2 + 2 + 2) return std::nullopt;
  if (!buffer.startsWith(kMagic)) return std::nullopt;
  if (buffer.at(3) != kCoderVersion) return std::nullopt;

  const auto metadataSize = readSize(buffer, 4);
  const auto compressedSize = readSize(buffer, 6 + metadataSize);
  const auto dataSize = readSize(buffer, 8 + metadataSize);
  const auto compressed = buffer.mid(10 + metadataSize, compressedSize);

  if (compressed.size() != compressedSize) return std::nullopt;

  const auto data = inflate(compressed, dataSize);

  if (data.isEmpty()) return std::nullopt;

  return filtersFromXml(QString::fromUtf8(data));
}

QString shortcode(const FilterAction value) {
  return toShortcode(kActions, value);
}

QString shortcode(const FilterElement value) {
  return toShortcode(kElements, value);
}

QString shortcode(const FilterMatch value) {
  return toShortcode(kMatches, value);
}

QString shortcode(const FilterOperator value) {
  return toShortcode(kOperators, value);
}

QString shortcode(const FilterOption value) {
  return toShortcode(kOptions, value);
}

std::optional<FilterAction> filterAction(const QString& shortcode) {
  return fromShortcode(kActions, shortcode);
}

std::optional<FilterElement> filterElement(const QString& shortcode) {
  return fromShortcode(kElements, shortcode);
}

std::optional<FilterMatch> filterMatch(const QString& shortcode) {
  return fromShortcode(kMatches, shortcode);
}

std::optional<FilterOperator> filterOperator(const QString& shortcode) {
  return fromShortcode(kOperators, shortcode);
}

std::optional<FilterOption> filterOption(const QString& shortcode) {
  return fromShortcode(kOptions, shortcode);
}

}  // namespace track::util
