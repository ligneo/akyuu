/**
 * Akyuu
 * Copyright (C) 2010-2026, Eren Okka
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

#include "recognition_relations.hpp"

#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <limits>

#include "akyuu/network.hpp"
#include "akyuu/path.hpp"
#include "base/file.hpp"
#include "base/log.hpp"
#include "base/string.hpp"
#include "sync/service.hpp"

namespace track::recognition {

namespace {

QString relationsFilePath() {
  return u"%1/anime-relations.txt"_s.arg(akyuu::get_data_path());
}

QString readRelationsFile() {
  // The data directory holds a newer copy once one is fetched, so it comes first. Until then the
  // bundled copy is used, the same way media player data is.
  const auto path = relationsFilePath();

  if (const auto contents = base::readFile(path); !contents.isEmpty()) {
    return contents;
  }

  const auto contents = base::readFile(":/anime-relations.txt");
  if (contents.isEmpty()) {
    qWarning() << "Could not read anime relations data";
  }

  return contents;
}

std::unordered_map<int, Relations>& relationsCache() {
  static std::unordered_map<int, Relations> cache;
  return cache;
}

Relations& relationsFor(const int column) {
  auto [it, inserted] = relationsCache().try_emplace(column);
  if (inserted) it->second.load(readRelationsFile(), column);

  return it->second;
}

std::optional<int> serviceIdColumn(const sync::ServiceId serviceId) {
  switch (serviceId) {
    case sync::ServiceId::MyAnimeList:
      return 0;
    case sync::ServiceId::Kitsu:
      return 1;
    case sync::ServiceId::AniList:
      return 2;
    default:
      return std::nullopt;
  }
}

}  // namespace

void Relations::addRule(const int id, const std::pair<int, int>& source_range,
                        const int destination_id, const std::pair<int, int>& destination_range) {
  rules_[id].push_back({destination_id, source_range, destination_range});
}

void Relations::parseRule(const QString& rule, const int column) {
  // See deps/anime-relations/README.md
  static const QRegularExpression pattern{
      // clang-format off
      R"(^(?<src_ids>(?:\d+|[?~])(?:\|(?:\d+|[?~]))*):(?<src_start>\d+)(?:-(?<src_end>\d+|\?))? -> )"
       R"((?<dst_ids>(?:\d+|[?~])(?:\|(?:\d+|[?~]))*):(?<dst_start>\d+)(?:-(?<dst_end>\d+|\?))?(?<bang>!)?$)"
      // clang-format on
  };

  const auto match = pattern.match(rule);
  if (!match.hasMatch()) {
    qWarning() << "Could not parse anime relations rule:" << rule;
    return;
  }

  const auto getId = [&](const QString& group) -> std::optional<int> {
    const auto ids = match.captured(group).split(u'|');
    if (column >= ids.size()) return std::nullopt;
    bool ok = false;
    const int value = ids.at(column).toInt(&ok);
    // `!ok` means we captured `?` or `~`, which is handled differently
    // for source and destination IDs.
    return ok ? std::optional{value} : std::nullopt;
  };

  const auto getRange = [&](const QString& first, const QString& second) -> std::pair<int, int> {
    const int start = match.captured(first).toInt();
    const auto end = match.captured(second);
    if (end.isEmpty()) return {start, start};
    bool ok = false;
    const int value = end.toInt(&ok);
    // `!ok` means we captured `?`, so the range is open-ended.
    return {start, ok ? value : std::numeric_limits<int>::max()};
  };

  const auto sourceId = getId("src_ids");
  if (!sourceId) return;  // cannot work with missing source ID
  const auto sourceRange = getRange("src_start", "src_end");

  // Repeat the source ID if destination ID is missing (i.e. `~`).
  const auto destinationId = getId("dst_ids").value_or(*sourceId);
  const auto destinationRange = getRange("dst_start", "dst_end");

  addRule(*sourceId, sourceRange, destinationId, destinationRange);

  if (!match.captured("bang").isEmpty()) {
    // `!` shorthand: Redirect destination to itself.
    addRule(destinationId, sourceRange, destinationId, destinationRange);
  }
}

void Relations::load(const QString& contents, const int column) {
  bool inRulesSection = false;

  for (auto line : contents.split(u'\n')) {
    line = line.trimmed();

    if (line.isEmpty() || line.startsWith(u'#')) continue;

    if (line.startsWith("::")) {
      inRulesSection = (line == "::rules");
      continue;
    }

    if (!inRulesSection) continue;

    if (line.startsWith(u'-')) line = line.mid(1).trimmed();

    parseRule(line, column);
  }
}

std::optional<Redirection> Relations::find(const int id, const int episode_number) const {
  const auto it = rules_.find(id);
  if (it == rules_.end()) return std::nullopt;

  for (const auto& rule : it->second) {
    const int distance = episode_number - rule.source_range.first;
    if (distance < 0) continue;
    if (episode_number > rule.source_range.second) continue;

    int destination = rule.destination_range.first;
    if (rule.destination_range.first != rule.destination_range.second) destination += distance;
    if (destination > rule.destination_range.second) continue;

    return Redirection{
        .id = rule.destination_id,
        .episode_range = {destination, destination},
    };
  }

  return std::nullopt;
}

std::optional<Redirection> findRedirection(const int id, const std::pair<int, int>& episode_range) {
  const auto column = serviceIdColumn(sync::currentServiceId());
  if (!column) return std::nullopt;

  const auto& relations = relationsFor(*column);

  const auto first = relations.find(id, episode_range.first);
  if (!first) return std::nullopt;

  if (episode_range.first == episode_range.second) return first;

  const auto second = relations.find(id, episode_range.second);
  if (!second) return std::nullopt;
  if (first->id != second->id) return std::nullopt;  // must redirect to the same anime

  return Redirection{
      .id = first->id,
      .episode_range = {first->episode_range.first, second->episode_range.first},
  };
}

namespace {

// The `last_modified` date in the file's `::meta` section, e.g. "2026-08-03"
QString relationsLastModified(const QString& contents) {
  static const QRegularExpression pattern{u"^- last_modified: (\\d{4}-\\d{2}-\\d{2})"_s,
                                          QRegularExpression::MultilineOption};
  return pattern.match(contents).captured(1);
}

}  // namespace

// v1 checks for newer relation data along with its own updates (`update.cpp:109`). The file lives
// in its own repository and changes more often than we release, so a newer copy is fetched into
// the data directory, where it takes precedence over the bundled one.
void updateRelations() {
  static const QUrl url{
      u"https://raw.githubusercontent.com/erengy/anime-relations/master/anime-relations.txt"_s};

  QNetworkRequest request{url};
  request.setHeaders(akyuu::NetworkAccessManager::commonHeaders());

  const auto reply = akyuu::network()->get(request);

  QObject::connect(reply, &QNetworkReply::finished, reply, [reply]() {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      qWarning() << "Could not check for anime relations:" << reply->errorString();
      return;
    }

    const auto contents = QString::fromUtf8(reply->readAll());
    const auto latest = relationsLastModified(contents);
    const auto current = relationsLastModified(readRelationsFile());

    // ISO dates compare correctly as strings
    if (latest.isEmpty() || !contents.contains(u"::rules"_s)) {
      qWarning() << "Downloaded anime relations data is not valid";
      return;
    }
    if (latest <= current) return;

    QSaveFile file{relationsFilePath()};
    if (!file.open(QIODevice::WriteOnly) || file.write(contents.toUtf8()) < 0 || !file.commit()) {
      qWarning() << "Could not save anime relations data:" << file.errorString();
      return;
    }

    relationsCache().clear();
    qDebug() << "Updated anime relations:" << current << "->" << latest;
  });
}

}  // namespace track::recognition
