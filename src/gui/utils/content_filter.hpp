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
// and which genres to require or exclude. Both live in the settings, and every page that changes
// them goes through here, so the other pages follow at once.
class ContentFilter final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ContentFilter)

public:
  static ContentFilter& instance();

  akyuu::AdultTitles adultTitles() const;
  const anime::GenreRules& genreRules() const;
  void setAdultTitles(akyuu::AdultTitles mode);
  void setGenreRules(const anime::GenreRules& rules);

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
  // A cover that was clicked stays revealed until Akyuu is closed
  QSet<int> m_revealed;
};

inline ContentFilter& contentFilter() {
  return ContentFilter::instance();
}

// The same three choices as in the settings, for the View menu of each page
void addAdultTitlesMenu(QMenu* menu);

}  // namespace gui
