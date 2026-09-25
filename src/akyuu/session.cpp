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

#include "session.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "base/string.hpp"
#include "gui/common/anime_list_view_base.hpp"
#include "gui/models/anime_list_model.hpp"
#include "gui/models/anime_list_proxy_model.hpp"
#include "media/anime_season.hpp"
#include "akyuu/path.hpp"

namespace taiga {

QString Session::fileName() const {
  return u"%1/session.json"_s.arg(get_data_path());
}

////////////////////////////////////////////////////////////////////////////////

namespace {

// Columns are kept as a list of numbers; an absent key means the view keeps its own defaults.
std::optional<QList<int>> toColumns(const QVariant& value) {
  if (!value.isValid()) return std::nullopt;
  QList<int> columns;
  for (const auto& column : value.toList()) columns.append(column.toInt());
  return columns;
}

QVariantList fromColumns(const QList<int>& columns) {
  QVariantList list;
  for (const auto column : columns) list.append(column);
  return list;
}

}  // namespace

std::optional<QList<int>> Session::animeListHiddenColumns() const {
  return toColumns(value("animeList.hiddenColumns"));
}

int Session::animeListSortColumn() const {
  return value("animeList.sortColumn", gui::AnimeListModel::COLUMN_TITLE).toInt();
}

Qt::SortOrder Session::animeListSortOrder() const {
  return value("animeList.sortOrder", Qt::SortOrder::AscendingOrder).value<Qt::SortOrder>();
}

gui::ListViewMode Session::animeListViewMode() const {
  return value("animeList.viewMode", static_cast<int>(gui::ListViewMode::List))
      .value<gui::ListViewMode>();
}

QByteArray Session::mainWindowGeometry() const {
  return QByteArray::fromBase64(value("mainWindow.geometry", QByteArray{}).toByteArray());
}

QByteArray Session::mediaDialogGeometry() const {
  return QByteArray::fromBase64(value("mediaDialog.geometry", QByteArray{}).toByteArray());
}

QByteArray Session::mediaDialogSplitterState() const {
  return QByteArray::fromBase64(value("mediaDialog.splitterState", QByteArray{}).toByteArray());
}

// The season being browsed is the subject of the page, not a hidden filter, so it is remembered
// across runs. v1 does the same (`program/seasons/lastseason`).
anime::Season Session::season() const {
  const auto name = value("seasons.name").toInt();
  const auto year = value("seasons.year").toInt();

  if (!name || !year) return anime::Season{QDate::currentDate().toStdSysDays()};

  return anime::Season{static_cast<anime::SeasonName>(name), std::chrono::year{year}};
}

// v1 counts this in its statistics as well (`stats.tigers_harmed`), and the joke only works if
// the number survives a restart.
int Session::tigersHarmed() const {
  return value("tigersHarmed", 0).toInt();
}

gui::AnimeListGroupBy Session::seasonsGroupBy() const {
  return static_cast<gui::AnimeListGroupBy>(
      value("seasons.groupBy", static_cast<int>(gui::AnimeListGroupBy::Type)).toInt());
}

int Session::seasonsSortColumn() const {
  return value("seasons.sortColumn", 0).toInt();
}

Qt::SortOrder Session::seasonsSortOrder() const {
  return static_cast<Qt::SortOrder>(value("seasons.sortOrder", 0).toInt());
}

gui::ListViewMode Session::seasonsViewMode() const {
  return static_cast<gui::ListViewMode>(
      value("seasons.viewMode", static_cast<int>(gui::ListViewMode::Cards)).toInt());
}

gui::AnimeListProxyModelFilter Session::searchListFilters() const {
  const auto json = QJsonDocument::fromJson(
      QByteArray::fromBase64(value("searchList.filters", QByteArray{}).toByteArray()));

  // clang-format off
  if (json.isEmpty()) {
    return gui::AnimeListProxyModelFilter{
        .year   = QDate::currentDate().year(),
        .season = static_cast<int>(anime::Season{QDate::currentDate().toStdSysDays()}.name),
        .type   = static_cast<int>(anime::Type::Tv),
    };
  } else {
    static const auto optionalInt = [](const QJsonValue& value) {
      return value.isDouble() ? value.toInt() : std::optional<int>{};
    };
    return gui::AnimeListProxyModelFilter{
        .year   = optionalInt(json["year"]),
        .season = optionalInt(json["season"]),
        .type   = optionalInt(json["type"]),
        .status = optionalInt(json["status"]),
        .listStatus = {
            .status = optionalInt(json["listStatus"]),
        },
    };
  }
  // clang-format on
}

std::optional<QList<int>> Session::searchListHiddenColumns() const {
  return toColumns(value("searchList.hiddenColumns"));
}

int Session::searchListSortColumn() const {
  return value("searchList.sortColumn", gui::AnimeListModel::COLUMN_AVERAGE).toInt();
}

Qt::SortOrder Session::searchListSortOrder() const {
  return value("searchList.sortOrder", Qt::SortOrder::DescendingOrder).value<Qt::SortOrder>();
}

gui::ListViewMode Session::searchListViewMode() const {
  return value("searchList.viewMode", static_cast<int>(gui::ListViewMode::Cards))
      .value<gui::ListViewMode>();
}

////////////////////////////////////////////////////////////////////////////////

void Session::setAnimeListHiddenColumns(const QList<int>& columns) const {
  setValue("animeList.hiddenColumns", fromColumns(columns));
}

void Session::setAnimeListSortColumn(const int column) const {
  setValue("animeList.sortColumn", column);
}

void Session::setAnimeListSortOrder(const Qt::SortOrder order) const {
  setValue("animeList.sortOrder", order);
}

void Session::setAnimeListViewMode(const gui::ListViewMode mode) const {
  setValue("animeList.viewMode", static_cast<int>(mode));
}

void Session::setMainWindowGeometry(const QByteArray& geometry) const {
  setValue("mainWindow.geometry", geometry.toBase64().toStdString());
}

void Session::setMediaDialogGeometry(const QByteArray& geometry) const {
  setValue("mediaDialog.geometry", geometry.toBase64().toStdString());
}

void Session::setMediaDialogSplitterState(const QByteArray& state) const {
  setValue("mediaDialog.splitterState", state.toBase64().toStdString());
}

void Session::setSeason(const anime::Season season) const {
  setValue("seasons.name", static_cast<int>(season.name));
  setValue("seasons.year", static_cast<int>(season.year));
}

void Session::setTigersHarmed(const int count) const {
  setValue("tigersHarmed", count);
}

void Session::setSeasonsGroupBy(const gui::AnimeListGroupBy groupBy) const {
  setValue("seasons.groupBy", static_cast<int>(groupBy));
}

void Session::setSeasonsSortColumn(const int column) const {
  setValue("seasons.sortColumn", column);
}

void Session::setSeasonsSortOrder(const Qt::SortOrder order) const {
  setValue("seasons.sortOrder", static_cast<int>(order));
}

void Session::setSeasonsViewMode(const gui::ListViewMode mode) const {
  setValue("seasons.viewMode", static_cast<int>(mode));
}

void Session::setSearchListFilters(const gui::AnimeListProxyModelFilter& filters) const {
  // clang-format off
  QJsonObject object{
      {"year",   filters.year   ? *filters.year   : QJsonValue{}},
      {"season", filters.season ? *filters.season : QJsonValue{}},
      {"type",   filters.type   ? *filters.type   : QJsonValue{}},
      {"status", filters.status ? *filters.status : QJsonValue{}},
  };
  // clang-format on
  if (filters.listStatus.status) {
    object["listStatus"] = *filters.listStatus.status;
  }
  setValue("searchList.filters",
           QJsonDocument{object}.toJson(QJsonDocument::Compact).toBase64().toStdString());
}

void Session::setSearchListHiddenColumns(const QList<int>& columns) const {
  setValue("searchList.hiddenColumns", fromColumns(columns));
}

void Session::setSearchListSortColumn(const int column) const {
  setValue("searchList.sortColumn", column);
}

void Session::setSearchListSortOrder(const Qt::SortOrder order) const {
  setValue("searchList.sortOrder", order);
}

void Session::setSearchListViewMode(const gui::ListViewMode mode) const {
  setValue("searchList.viewMode", static_cast<int>(mode));
}

}  // namespace taiga
