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

#include "format.hpp"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <cmath>
#include <format>

#include "base/chrono.hpp"
#include "base/string.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "media/anime_list.hpp"
#include "media/anime_season.hpp"
#include "media/anime_utils.hpp"
#include "track/feed_filter.hpp"

namespace gui {

QString formatNumber(const int value, QString placeholder) {
  return value > 0 ? QString::number(value) : placeholder;
}

QString formatEpisodeLength(const int minutes, QString placeholder) {
  return minutes > 0 ? u"%1m"_s.arg(minutes) : placeholder;
}

QString formatEpisodeNumbers(const std::vector<std::string>& numbers, QString placeholder) {
  if (numbers.empty()) return placeholder;

  // Strip leading zeros (e.g. "01" -> "1")
  const auto normalize = [](const std::string& value) {
    if (value == "0") return value;  // leave "0" alone
    const auto pos = value.find_first_not_of('0');
    return pos == std::string::npos ? value : value.substr(pos);
  };

  const auto first = normalize(numbers.front());
  const auto last = normalize(numbers.back());

  if (first == last) return QString::fromStdString(first);

  return u"%1-%2"_s.arg(first).arg(last);
}

QString formatScore(const double value) {
  return u"%1%"_s.arg(value * 10.0, 0, 'g', 4);
}

QString formatDate(const base::Date& date, QString placeholder) {
  return date.ok() ? formatDate(QDate(date), placeholder) : placeholder;
}

QString formatDate(const QDate date, QString placeholder) {
  return date.isValid() ? QDate(date).toString(Qt::RFC2822Date) : placeholder;
}

QString formatFuzzyDate(const base::FuzzyDate& fuzzyDate, QString placeholder) {
  const QDate date(fuzzyDate.year(), fuzzyDate.month(), fuzzyDate.day());
  return fuzzyDate ? date.toString(Qt::RFC2822Date) : placeholder;
}

QString formatFuzzyDateRange(const base::FuzzyDate& from, const base::FuzzyDate& to,
                             QString placeholder) {
  if (from == to) return formatFuzzyDate(from, placeholder);
  return u"%1 to %2"_s.arg(formatFuzzyDate(from, placeholder))
      .arg(formatFuzzyDate(to, placeholder));
}

QString formatAsRelativeTime(const qint64 time, QString placeholder) {
  if (!time) return placeholder;

  const QDateTime datetime = QDateTime::fromSecsSinceEpoch(time);
  const QDateTime now = QDateTime::currentDateTimeUtc();
  const auto timeDiff = datetime.secsTo(now);
  const Duration duration(std::chrono::seconds{std::abs(timeDiff)});

  const QString str = [&duration]() {
    // clang-format off
    if (duration.seconds() < 90) {
      return QCoreApplication::translate("gui/utils/format", "a moment");
    }
    if (duration.minutes() < 45) {
      const auto n = std::lround(duration.minutes());
      return QCoreApplication::translate("gui/utils/format", "%n minute(s)", nullptr, n);
    }
    if (duration.hours() < 22) {
      const auto n = std::lround(duration.hours());
      return QCoreApplication::translate("gui/utils/format", "%n hour(s)", nullptr, n);
    }
    if (duration.days() < 25) {
      const auto n = std::lround(duration.days());
      return QCoreApplication::translate("gui/utils/format", "%n day(s)", nullptr, n);
    }
    if (duration.days() < 345) {
      const auto n = std::lround(duration.months());
      return QCoreApplication::translate("gui/utils/format", "%n month(s)", nullptr, n);
    }
    {
      const auto n = std::lround(duration.years());
      return QCoreApplication::translate("gui/utils/format", "%n year(s)", nullptr, n);
    }
    // clang-format on
  }();

  return timeDiff < 0 ? u"in %1"_s.arg(str) : u"%1 ago"_s.arg(str);
}

QString formatDuration(Duration duration) {
  const auto hours = static_cast<int>(duration.hours());
  duration = Duration(std::chrono::seconds{duration.seconds() % Duration::hours_t::period::num});

  const auto minutes = static_cast<int>(duration.minutes());
  duration = Duration(std::chrono::seconds{duration.seconds() % Duration::minutes_t::period::num});

  const auto seconds = duration.seconds();

  if (hours > 0) {
    return QString::fromStdString(std::format("{:0>2}:{:0>2}:{:0>2}", hours, minutes, seconds));
  } else {
    return QString::fromStdString(std::format("{:0>2}:{:0>2}", minutes, seconds));
  }
}

QString formatTimeSpan(Duration duration) {
  const auto days = static_cast<int>(duration.days());
  const auto hours = static_cast<int>(duration.hours()) % 24;
  const auto minutes = static_cast<int>(duration.minutes()) % 60;

  QStringList parts;
  if (days > 0)
    parts.append(QCoreApplication::translate("gui/utils/format", "%n day(s)", nullptr, days));
  if (hours > 0)
    parts.append(QCoreApplication::translate("gui/utils/format", "%n hour(s)", nullptr, hours));
  if (minutes > 0)
    parts.append(QCoreApplication::translate("gui/utils/format", "%n minute(s)", nullptr, minutes));

  if (parts.isEmpty()) return QCoreApplication::translate("gui/utils/format", "None");

  return parts.join(' ');
}

QString formatTimestamp(const qint64 time) {
  const QDateTime datetime = QDateTime::fromSecsSinceEpoch(time);
  return formatDate(datetime.date());
}

QString formatTransferProgress(const qint64 current, const qint64 total) {
  if (total > 0) {
    return u"%1%"_s.arg(static_cast<int>(100.0 * current / total));  // e.g. "45%"
  }
  return QLocale::system().formattedDataSize(current, 1);  // e.g. "1.2 MiB"
}

QString formatSeason(const anime::Season season, QString placeholder) {
  if (!season.has_name() && !season.has_year()) return placeholder;
  if (!season.has_name()) {
    return u"%1"_s.arg(static_cast<int>(season.year));
  } else {
    return u"%1 %2"_s.arg(formatSeasonName(season.name)).arg(static_cast<int>(season.year));
  }
}

QString formatSeasonName(const anime::SeasonName name) {
  using enum anime::SeasonName;

  // clang-format off
  switch (name) {
    default:
    case Unknown: return "Unknown";
    case Winter: return "Winter";
    case Spring: return "Spring";
    case Summer: return "Summer";
    case Fall: return "Fall";
  }
  // clang-format on
}

QString formatStatus(const anime::Status value) {
  using enum anime::Status;

  // clang-format off
  switch (value) {
    case Unknown: return "Unknown";
    case Airing: return "Airing";
    case FinishedAiring: return "Finished";
    case NotYetAired: return "Not yet aired";
    default: return "";
  }
  // clang-format on
}

QString formatType(const anime::Type value) {
  using enum anime::Type;

  // clang-format off
  switch (value) {
    case Unknown: return "Unknown";
    case Tv: return "TV";
    case Ova: return "OVA";
    case Movie: return "Movie";
    case Special: return "Special";
    case Ona: return "ONA";
    case Music: return "Music";
    default: return "";
  }
  // clang-format on
}

QString formatListStatus(const anime::list::Status value) {
  using enum anime::list::Status;

  // clang-format off
  switch (value) {
    case NotInList: return "Not in list";
    case Watching: return "Watching";
    case Completed: return "Completed";
    case OnHold: return "Paused";
    case Dropped: return "Dropped";
    case PlanToWatch: return "Planning";
    default: return "";
  }
  // clang-format on
}

QString formatFilterAction(const track::FilterAction value) {
  using enum track::FilterAction;

  // clang-format off
  switch (value) {
    case Discard: return "Discard matched items";
    case Select: return "Select matched items";
    case Prefer: return "Prefer matched items to similar ones";
    default: return "";
  }
  // clang-format on
}

QString formatFilterElement(const track::FilterElement value) {
  using enum track::FilterElement;

  // clang-format off
  switch (value) {
    case FileTitle: return "File name";
    case FileCategory: return "File category";
    case FileDescription: return "File description";
    case FileLink: return "File link";
    case FileSize: return "File size";
    case MetaId: return "Anime ID";
    case EpisodeTitle: return "Episode title";
    case MetaDateStart: return "Anime date started";
    case MetaDateEnd: return "Anime date ended";
    case MetaEpisodes: return "Anime episode count";
    case MetaStatus: return "Anime airing status";
    case MetaType: return "Anime type";
    case UserNotes: return "Anime notes";
    case UserStatus: return "Anime watching status";
    case EpisodeNumber: return "Episode number";
    case EpisodeVersion: return "Episode version";
    case LocalEpisodeAvailable: return "Episode availability";
    case EpisodeGroup: return "Episode fansub group";
    case EpisodeVideoResolution: return "Episode video resolution";
    case EpisodeVideoType: return "Episode video type";
    default: return "";
  }
  // clang-format on
}

QString formatFilterMatch(const track::FilterMatch value) {
  using enum track::FilterMatch;

  // clang-format off
  switch (value) {
    case All: return "All conditions";
    case Any: return "Any condition";
    default: return "";
  }
  // clang-format on
}

QString formatFilterOperator(const track::FilterOperator value) {
  using enum track::FilterOperator;

  // clang-format off
  switch (value) {
    case Equals: return "is";
    case NotEquals: return "is not";
    case IsGreaterThan: return "is greater than";
    case IsGreaterThanOrEqualTo: return "is greater than or equal to";
    case IsLessThan: return "is less than";
    case IsLessThanOrEqualTo: return "is less than or equal to";
    case BeginsWith: return "begins with";
    case EndsWith: return "ends with";
    case Contains: return "contains";
    case NotContains: return "does not contain";
    default: return "";
  }
  // clang-format on
}

QString formatFilterOption(const track::FilterOption value) {
  using enum track::FilterOption;

  // clang-format off
  switch (value) {
    case Default: return "Default";
    case Deactivate: return "Deactivate discarded items";
    case Hide: return "Hide discarded items";
    default: return "";
  }
  // clang-format on
}

// Values that are stored as a number are shown by name instead, as in v1.
QString formatFilterValue(const track::FilterCondition& condition) {
  const auto value = QString::fromStdString(condition.value);

  switch (condition.element) {
    case track::FilterElement::MetaId: {
      if (value.isEmpty()) return "(?)";
      const auto item = anime::db.item(value.toInt());
      return u"%1 (%2)"_s.arg(value).arg(item ? QString::fromStdString(anime::preferredTitle(*item))
                                              : u"?"_s);
    }
    case track::FilterElement::UserStatus:
      return formatListStatus(static_cast<anime::list::Status>(value.toInt()));
    case track::FilterElement::MetaStatus:
      return formatStatus(static_cast<anime::Status>(value.toInt()));
    case track::FilterElement::MetaType:
      return formatType(static_cast<anime::Type>(value.toInt()));
    default:
      return !value.isEmpty() ? value : u"(empty)"_s;
  }
}

QString formatFilterCondition(const track::FilterCondition& condition) {
  return u"%1 %2 \"%3\""_s.arg(formatFilterElement(condition.element))
      .arg(formatFilterOperator(condition.op))
      .arg(formatFilterValue(condition));
}

}  // namespace gui
