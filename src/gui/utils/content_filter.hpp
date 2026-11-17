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

#pragma once

#include <QObject>
#include <QSet>
#include <QStringList>

class QMenu;

#include "akyuu/settings.hpp"
#include "media/genre_rules.hpp"

namespace gui {

// The Seasons and Search pages share one set of content filters: what to do with adult titles
// and which types and genres to require or exclude. The settings hold the defaults,
// which Akyuu starts with. The Filters button and the View menu change the filters in use until
// Akyuu is closed:
// a look at "only Sports" should not quietly narrow every season from then on, and an adult
// mode switched to Show for a moment goes back to the default by itself.
class ContentFilter final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ContentFilter)

public:
  static ContentFilter& instance();

  // In use, possibly changed for now
  akyuu::AdultTitles adultTitles() const;
  const anime::GenreRules& genreRules() const;
  const anime::TypeRules& typeRules() const;
  void setAdultTitles(akyuu::AdultTitles mode);
  void setGenreRules(const anime::GenreRules& rules);
  // Both at once, so that the pages filter once
  void setFilters(const anime::GenreRules& genres, const anime::TypeRules& types);

  // Kept in the settings. Setting them applies them at once as well.
  akyuu::AdultTitles defaultAdultTitles() const;
  anime::GenreRules defaultGenreRules() const;
  anime::TypeRules defaultTypeRules() const;
  void setDefaults(akyuu::AdultTitles mode, const anime::GenreRules& genres,
                   const anime::TypeRules& types);

  bool adultTitlesChanged() const;
  // The types or the genres, which share the Filters button
  bool filtersChanged() const;
  void resetToDefaults();

  bool accepts(const Anime& anime) const;
  bool blurs(const Anime& anime) const;
  void reveal(int id);

  // Every genre Akyuu has seen, plus the ones in the rules even if no title has them anymore
  QStringList knownGenres() const;

signals:
  void changed();
  void revealed(int id);

private:
  ContentFilter();

  akyuu::AdultTitles m_adultTitles = akyuu::AdultTitles::Hide;
  anime::GenreRules m_genreRules;
  anime::TypeRules m_typeRules;
  // A cover that was clicked stays revealed until Akyuu is closed
  QSet<int> m_revealed;
};

inline ContentFilter& contentFilter() {
  return ContentFilter::instance();
}

// The same three choices as in the settings, for the View menu of each page
void addAdultTitlesMenu(QMenu* menu);

}  // namespace gui
