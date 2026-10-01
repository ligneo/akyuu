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

#include "rss.hpp"

#include <QXmlStreamReader>

namespace rss {

namespace {

// Some feeds nest elements in item fields (e.g. `media:group`, or XHTML left unescaped in the
// description). Reading only the text would put the reader in an error state and lose the feed.
std::string text(QXmlStreamReader& xml) {
  return xml.readElementText(QXmlStreamReader::IncludeChildElements).toStdString();
}

std::string attribute(const QXmlStreamReader& xml, QAnyStringView name) {
  return xml.attributes().value(name).toString().toStdString();
}

Item parseItem(QXmlStreamReader& xml) {
  Item item;

  while (xml.readNextStartElement()) {
    const auto name = xml.qualifiedName().toString();

    if (name == u"title") {
      item.title = text(xml);

    } else if (name == u"link") {
      item.link = text(xml);

    } else if (name == u"description") {
      item.description = text(xml);

    } else if (name == u"author") {
      item.author = text(xml);

    } else if (name == u"comments") {
      item.comments = text(xml);

    } else if (name == u"pubDate") {
      item.pub_date = text(xml);

    } else if (name == u"category") {
      item.category.domain = attribute(xml, u"domain");
      item.category.value = text(xml);

    } else if (name == u"enclosure") {
      item.enclosure.url = attribute(xml, u"url");
      item.enclosure.length = attribute(xml, u"length");
      item.enclosure.type = attribute(xml, u"type");
      xml.skipCurrentElement();

    } else if (name == u"guid") {
      item.guid.is_permalink = xml.attributes().value(u"isPermaLink") != u"false";
      item.guid.value = text(xml);

    } else if (name == u"source") {
      item.source.url = attribute(xml, u"url");
      item.source.name = text(xml);

    } else if (name.contains(u':')) {
      // Providers publish the file size, seeders and the like in their own namespace.
      item.namespace_elements[name.toStdString()] = text(xml);

    } else {
      xml.skipCurrentElement();
    }
  }

  return item;
}

}  // namespace

std::optional<Feed> parse(const QString& data) {
  QXmlStreamReader xml{data};

  // Qualified names are kept as they are (e.g. `nyaa:size`), so that provider-specific elements can
  // be told apart by their prefix.
  xml.setNamespaceProcessing(false);

  if (!xml.readNextStartElement() || xml.qualifiedName() != u"rss") {
    return std::nullopt;
  }

  Feed feed;

  while (xml.readNextStartElement()) {
    if (xml.qualifiedName() != u"channel") {
      xml.skipCurrentElement();
      continue;
    }

    while (xml.readNextStartElement()) {
      // Qualified, so that `atom:link` is not mistaken for the channel link.
      const auto name = xml.qualifiedName().toString();

      if (name == u"item") {
        auto item = parseItem(xml);
        // All elements of an item are optional, but at least one of title or description must be
        // present.
        if (!item.title.empty() || !item.description.empty()) {
          feed.items.push_back(std::move(item));
        }

      } else if (name == u"title") {
        feed.channel.title = text(xml);

      } else if (name == u"link") {
        feed.channel.link = text(xml);

      } else if (name == u"description") {
        feed.channel.description = text(xml);

      } else {
        xml.skipCurrentElement();
      }
    }
  }

  if (xml.hasError()) {
    return std::nullopt;
  }

  return feed;
}

}  // namespace rss
