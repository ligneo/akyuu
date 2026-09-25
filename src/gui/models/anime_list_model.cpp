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

#include "anime_list_model.hpp"

#include <QApplication>
#include <QBuffer>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QLocale>
#include <QPalette>
#include <QSize>
#include <algorithm>

#include "akyuu/settings.hpp"
#include "gui/utils/format.hpp"
#include "gui/utils/image_provider.hpp"
#include "gui/utils/rating.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list_utils.hpp"
#include "media/anime_season.hpp"
#include "media/anime_utils.hpp"
#include "track/library.hpp"

namespace gui {

namespace {

using namespace Qt::StringLiterals;

// Tooltips are rich text, so the poster travels inside the HTML. Anything bigger than this is a
// waste: the image is only there to recognize the title at a glance.
constexpr int kTooltipPosterWidth = 128;

QString posterHtml(const int id) {
  const auto poster = imageProvider.loadPoster(id);

  // The first load is asynchronous, so an anime whose poster has not been read yet simply gets a
  // tooltip without one. The next hover has it.
  if (poster.isNull()) return {};

  const auto scaled =
      poster.scaledToWidth(kTooltipPosterWidth, Qt::TransformationMode::SmoothTransformation);

  QByteArray data;
  QBuffer buffer(&data);
  buffer.open(QIODevice::WriteOnly);

  if (!scaled.save(&buffer, "PNG")) return {};

  return u"<img src=\"data:image/png;base64,%1\" width=\"%2\">"_s
      .arg(QString::fromLatin1(data.toBase64()))
      .arg(kTooltipPosterWidth);
}

// The title column is narrow and elides most names, so the tooltip carries the whole title plus
// the few things worth knowing before clicking.
// v1's `GetAvailableEpisodesTooltip`: which episodes are missing from the library folders, and
// where the airing has got to.
QString progressTooltip(const Anime& item, const ListEntry* entry) {
  const auto library = track::library();
  const int watched = entry ? entry->watched_episodes : 0;
  const int lastAvailable = library->lastAvailableEpisode(item.id);

  // v1's `GetLastEpisodeNumber`: the best guess at how many episodes there are by now
  int lastEpisode = item.episode_count;
  if (!anime::isFinishedAiring(item)) {
    lastEpisode = std::max({watched, lastAvailable, item.last_aired_episode,
                            anime::estimateLastAiredEpisodeNumber(item)});
  }
  const int count = std::max(lastEpisode, lastAvailable);

  QStringList lines;

  QList<std::pair<int, int>> missing;
  for (int number = 1; number <= count; ++number) {
    if (library->isEpisodeAvailable(item.id, number)) continue;
    if (!missing.isEmpty() && missing.back().second == number - 1) {
      missing.back().second = number;
    } else {
      missing.append({number, number});
    }
  }

  if (count > 0 && library->hasScanned()) {
    if (missing.size() == 1 && missing.front() == std::pair{1, count}) {
      lines.append(QCoreApplication::translate("gui", "All episodes are missing"));
    } else if (missing.isEmpty()) {
      lines.append(QCoreApplication::translate("gui", "All episodes are in library folders"));
    } else {
      QStringList ranges;
      for (const auto& [first, last] : missing) {
        ranges.append(first == last ? u"#%1"_s.arg(first) : u"#%1-%2"_s.arg(first).arg(last));
      }
      lines.append(QCoreApplication::translate("gui", "Missing: %1").arg(ranges.join(u", "_s)));
    }
  }

  if (!anime::isFinishedAiring(item)) {
    if (item.next_episode_time) {
      const auto time = QDateTime::fromSecsSinceEpoch(item.next_episode_time);
      lines.append(QCoreApplication::translate("gui", "Episode #%1 airing %2")
                       .arg(item.last_aired_episode + 1)
                       .arg(QLocale{}.toString(time, u"dddd HH:mm"_s)));
    } else if (const auto aired = anime::estimateLastAiredEpisodeNumber(item); aired > watched) {
      lines.append(QCoreApplication::translate("gui", "Aired: #%1 (estimated)").arg(aired));
    }
  }

  return lines.join(u'\n');
}

QString titleTooltip(const Anime& item) {
  QStringList lines;

  lines.append(
      u"<b>%1</b>"_s.arg(QString::fromStdString(anime::preferredTitle(item)).toHtmlEscaped()));

  // The same summary line the cards view draws, so the two read alike.
  QStringList facts{formatType(item.type)};
  if (item.episode_count != 1) {
    facts.append(QCoreApplication::translate("gui", "%1 episodes")
                     .arg(formatNumber(item.episode_count, "?")));
  }
  facts.append(formatSeason(anime::Season{item.date_started}, {}));
  facts.removeAll({});
  if (!facts.isEmpty()) lines.append(facts.join(u" · "_s).toHtmlEscaped());

  if (!item.titles.english.empty() && item.titles.english != item.titles.romaji) {
    lines.append(QString::fromStdString(item.titles.english).toHtmlEscaped());
  }

  const auto text = u"<div style='margin-left:6px'>%1</div>"_s.arg(lines.join(u"<br>"_s));
  const auto poster = posterHtml(item.id);

  if (poster.isEmpty()) return text;

  // A table keeps the poster and the text side by side without relying on flexbox, which Qt's
  // rich text engine does not have.
  return u"<table><tr><td>%1</td><td valign='top'>%2</td></tr></table>"_s.arg(poster).arg(text);
}

}  // namespace

AnimeListModel::AnimeListModel(QObject* parent) : QAbstractListModel(parent) {
  beginInsertRows({}, 0, anime::db.items().size());
  m_ids = anime::db.items().keys();
  endInsertRows();

  connect(&imageProvider, &ImageProvider::posterChanged, this, [this](int id) {
    if (const auto row = m_ids.indexOf(id); row > -1) {
      emit dataChanged(index(row), index(row), {static_cast<int>(AnimeListItemDataRole::Poster)});
    }
  });

  connect(&anime::db, &anime::Database::itemUpdated, this, &AnimeListModel::refreshRow);
  connect(&anime::db, &anime::Database::itemDeleted, this, &AnimeListModel::deleteRow);
  connect(&anime::db, &anime::Database::entryUpdated, this, &AnimeListModel::refreshRow);
  connect(&anime::db, &anime::Database::entryDeleted, this, &AnimeListModel::refreshRow);
}

void AnimeListModel::refreshRow(int id) {
  if (const auto row = m_ids.indexOf(id); row > -1) {
    emit dataChanged(index(row, 0), index(row, NUM_COLUMNS - 1));
  } else {
    addIds({id});
  }
}

void AnimeListModel::deleteRow(int id) {
  if (const auto row = m_ids.indexOf(id); row > -1) {
    beginRemoveRows({}, row, row);
    m_ids.removeAt(row);
    endRemoveRows();
  }
}

void AnimeListModel::addIds(const QList<int>& ids) {
  QList<int> newIds;
  for (const int id : ids) {
    if (!m_ids.contains(id)) newIds.append(id);
  }
  if (newIds.isEmpty()) return;

  const int first = m_ids.size();
  beginInsertRows({}, first, first + newIds.size() - 1);
  m_ids.append(newIds);
  endInsertRows();
}

int AnimeListModel::rowCount(const QModelIndex&) const {
  return m_ids.size();
}

int AnimeListModel::columnCount(const QModelIndex&) const {
  return NUM_COLUMNS;
}

QVariant AnimeListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) return {};

  const auto anime = getAnime(index);
  if (!anime) return {};

  const auto entry = getListEntry(index);

  switch (role) {
    case Qt::DisplayRole:
      switch (index.column()) {
        case COLUMN_TITLE:
          return QString::fromStdString(anime::preferredTitle(*anime));
        case COLUMN_DURATION:
          return formatEpisodeLength(anime->episode_length);
        case COLUMN_REWATCHES:
          if (entry) return entry->rewatched_times;
          break;
        case COLUMN_SCORE:
          if (entry) return formatRating(entry->score);
          break;
        case COLUMN_AVERAGE:
          return formatScore(anime->score);
        case COLUMN_TYPE:
          return formatType(anime->type);
        case COLUMN_SEASON:
          return formatSeason(anime::Season(anime->date_started));
        case COLUMN_STARTED:
          if (entry) return formatFuzzyDate(entry->date_started);
          break;
        case COLUMN_COMPLETED:
          if (entry) return formatFuzzyDate(entry->date_completed);
          break;
        case COLUMN_LAST_UPDATED:
          if (entry) return formatAsRelativeTime(entry->last_updated, "-");
          break;
        case COLUMN_NOTES:
          if (entry) return QString::fromStdString(entry->notes);
          break;
      }
      break;

    case Qt::FontRole:
      if (akyuu::settings.listHighlightNewEpisodes() && hasNewEpisode(*anime, entry)) {
        auto font = QApplication::font();
        font.setWeight(QFont::Weight::DemiBold);
        return font;
      }
      break;

    case Qt::ToolTipRole:
      switch (index.column()) {
        case COLUMN_TITLE:
          return titleTooltip(*anime);
        case COLUMN_PROGRESS: {
          const auto text = progressTooltip(*anime, entry);
          if (!text.isEmpty()) return text;
          break;
        }
        case COLUMN_SEASON:
          return formatFuzzyDate(anime->date_started);
        case COLUMN_LAST_UPDATED:
          if (entry) return formatTimestamp(entry->last_updated);
          break;
        case COLUMN_NOTES:
          if (entry) return QString::fromStdString(entry->notes);
          break;
      }
      break;

    case Qt::TextAlignmentRole: {
      switch (index.column()) {
        case COLUMN_PROGRESS:
        case COLUMN_REWATCHES:
        case COLUMN_SCORE:
        case COLUMN_AVERAGE:
        case COLUMN_TYPE:
          return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case COLUMN_DURATION:
        case COLUMN_SEASON:
        case COLUMN_STARTED:
        case COLUMN_COMPLETED:
        case COLUMN_LAST_UPDATED:
          return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        default:
          return {};
      }
      break;
    }

    case Qt::ForegroundRole: {
      const auto disabledTextColor =
          qApp->palette().color(QPalette::ColorGroup::Disabled, QPalette::ColorRole::Text);
      switch (index.column()) {
        case COLUMN_AVERAGE:
          if (!anime->score) return disabledTextColor;
          break;
        case COLUMN_DURATION:
          if (anime->episode_length < 1) return disabledTextColor;
          break;
        case COLUMN_SEASON:
          if (!anime->date_started) return disabledTextColor;
          break;
        case COLUMN_TYPE:
          if (anime->type == anime::Type::Unknown) return disabledTextColor;
          break;
        case COLUMN_SCORE:
          if (entry && !entry->score) return disabledTextColor;
          break;
        case COLUMN_STARTED:
          if (entry && !entry->date_started) return disabledTextColor;
          break;
        case COLUMN_COMPLETED:
          if (entry && !entry->date_completed) return disabledTextColor;
          break;
        case COLUMN_LAST_UPDATED:
          if (entry && !entry->last_updated) return disabledTextColor;
          break;
      }
      break;
    }

    case static_cast<int>(AnimeListItemDataRole::Anime): {
      return QVariant::fromValue(anime);
    }
    case static_cast<int>(AnimeListItemDataRole::ListEntry): {
      return QVariant::fromValue(entry);
    }
    case static_cast<int>(AnimeListItemDataRole::Poster): {
      return QVariant::fromValue(imageProvider.loadPoster(anime->id));
    }
  }

  return {};
}

bool AnimeListModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (index.isValid() && role == Qt::EditRole) {
    if (index.column() == COLUMN_SCORE) {
      const int id = m_ids.at(index.row());
      if (const auto entry = anime::db.entry(id)) {
        auto updated = *entry;
        updated.score = value.toInt();
        anime::list::save(updated);
      }
      return true;
    }
  }
  return false;
}

QVariant AnimeListModel::headerData(int section, Qt::Orientation orientation, int role) const {
  switch (role) {
    case Qt::DisplayRole: {
      // clang-format off
      switch (section) {
        case COLUMN_TITLE: return tr("Title");
        case COLUMN_PROGRESS: return tr("Progress");
        case COLUMN_DURATION: return tr("Duration");
        case COLUMN_REWATCHES: return tr("Rewatches");
        case COLUMN_SCORE: return tr("Score");
        case COLUMN_AVERAGE: return tr("Average");
        case COLUMN_TYPE: return tr("Type");
        case COLUMN_SEASON: return tr("Season");
        case COLUMN_STARTED: return tr("Started");
        case COLUMN_COMPLETED: return tr("Completed");
        case COLUMN_LAST_UPDATED: return tr("Last updated");
        case COLUMN_NOTES: return tr("Notes");
      }
      // clang-format on
      break;
    }

    case Qt::TextAlignmentRole: {
      switch (section) {
        case COLUMN_PROGRESS:
        case COLUMN_REWATCHES:
        case COLUMN_SCORE:
        case COLUMN_AVERAGE:
        case COLUMN_TYPE:
          return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
        case COLUMN_DURATION:
        case COLUMN_SEASON:
        case COLUMN_STARTED:
        case COLUMN_COMPLETED:
        case COLUMN_LAST_UPDATED:
          return QVariant(Qt::AlignRight | Qt::AlignVCenter);
      }
      break;
    }

    case Qt::InitialSortOrderRole: {
      switch (section) {
        case COLUMN_PROGRESS:
        case COLUMN_DURATION:
        case COLUMN_REWATCHES:
        case COLUMN_SCORE:
        case COLUMN_AVERAGE:
        case COLUMN_SEASON:
        case COLUMN_STARTED:
        case COLUMN_COMPLETED:
        case COLUMN_LAST_UPDATED:
          return Qt::DescendingOrder;
        default:
          return Qt::AscendingOrder;
      }
      break;
    }
  }

  return QAbstractListModel::headerData(section, orientation, role);
}

Qt::ItemFlags AnimeListModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) return Qt::NoItemFlags;

  return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

const Anime* AnimeListModel::getAnime(const QModelIndex& index) const {
  if (!index.isValid()) return nullptr;
  return anime::db.item(m_ids.at(index.row()));
}

const ListEntry* AnimeListModel::getListEntry(const QModelIndex& index) const {
  if (!index.isValid()) return nullptr;
  return anime::db.entry(m_ids.at(index.row()));
}

bool hasNewEpisode(const Anime& anime, const ListEntry* entry) {
  if (!anime::list::isInList(entry)) return false;
  return track::library()->isEpisodeAvailable(anime.id, entry->watched_episodes + 1);
}

}  // namespace gui
