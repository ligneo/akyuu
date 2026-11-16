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

#include "content_filter.hpp"

#include <QActionGroup>
#include <QCollator>
#include <QMenu>
#include <QSet>
#include <QTimer>

#include "akyuu/session.hpp"
#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "media/anime_utils.hpp"

namespace gui {

ContentFilter& ContentFilter::instance() {
  static ContentFilter filter;
  return filter;
}

ContentFilter::ContentFilter() {
  // 0.3.0 kept the switch with the Seasons page. Whoever turned it off there wanted to see them.
  if (!akyuu::settings.hasAdultTitles() && akyuu::session.seasonsHideNsfw() == false) {
    akyuu::settings.setAdultTitles(akyuu::AdultTitles::Show);
  }

  m_adultTitles = defaultAdultTitles();
  m_genreRules = defaultGenreRules();
}

namespace {

bool sameRules(const anime::GenreRules& a, const anime::GenreRules& b) {
  return a.required == b.required && a.excluded == b.excluded;
}

}  // namespace

akyuu::AdultTitles ContentFilter::adultTitles() const {
  return m_adultTitles;
}

const anime::GenreRules& ContentFilter::genreRules() const {
  return m_genreRules;
}

void ContentFilter::setAdultTitles(const akyuu::AdultTitles mode) {
  if (m_adultTitles == mode) return;
  m_adultTitles = mode;
  emit changed();
}

void ContentFilter::setGenreRules(const anime::GenreRules& rules) {
  if (sameRules(m_genreRules, rules)) return;
  m_genreRules = rules;
  emit changed();
}

akyuu::AdultTitles ContentFilter::defaultAdultTitles() const {
  return akyuu::settings.adultTitles();
}

anime::GenreRules ContentFilter::defaultGenreRules() const {
  return {
      .required = akyuu::settings.requiredGenres(),
      .excluded = akyuu::settings.excludedGenres(),
  };
}

void ContentFilter::setDefaults(const akyuu::AdultTitles mode, const anime::GenreRules& rules) {
  akyuu::settings.setAdultTitles(mode);
  akyuu::settings.setRequiredGenres(rules.required);
  akyuu::settings.setExcludedGenres(rules.excluded);
  m_adultTitles = mode;
  m_genreRules = rules;
  emit changed();
}

bool ContentFilter::adultTitlesChanged() const {
  return m_adultTitles != defaultAdultTitles();
}

bool ContentFilter::genreRulesChanged() const {
  return !sameRules(m_genreRules, defaultGenreRules());
}

void ContentFilter::resetToDefaults() {
  m_adultTitles = defaultAdultTitles();
  m_genreRules = defaultGenreRules();
  emit changed();
}

bool ContentFilter::accepts(const Anime& anime) const {
  if (m_adultTitles == akyuu::AdultTitles::Hide && anime::isNsfw(anime)) return false;
  return anime::matchesGenreRules(anime.genres, m_genreRules);
}

bool ContentFilter::blurs(const Anime& anime) const {
  return m_adultTitles == akyuu::AdultTitles::Blur && anime::isNsfw(anime) &&
         !m_revealed.contains(anime.id);
}

void ContentFilter::reveal(const int id) {
  if (m_revealed.contains(id)) return;
  m_revealed.insert(id);
  emit revealed(id);
}

QStringList ContentFilter::knownGenres() const {
  QSet<QString> genres;
  for (const auto& item : anime::db.items()) {
    for (const auto& genre : item.genres) genres.insert(QString::fromStdString(genre));
  }
  for (const auto& list : {m_genreRules.required, m_genreRules.excluded}) {
    for (const auto& genre : list) genres.insert(QString::fromStdString(genre));
  }

  auto list = QStringList{genres.begin(), genres.end()};
  QCollator collator;
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  std::ranges::sort(list, collator);
  return list;
}

void addAdultTitlesMenu(QMenu* menu) {
  const QList<QPair<QString, akyuu::AdultTitles>> modes{
      {QObject::tr("Show"), akyuu::AdultTitles::Show},
      {QObject::tr("Blur"), akyuu::AdultTitles::Blur},
      {QObject::tr("Hide"), akyuu::AdultTitles::Hide},
  };

  // The current mode is in the title, so that its mark is not mistaken for a second choice of
  // the menu it opens from (List or Cards)
  QString current;
  for (const auto& [text, mode] : modes) {
    if (mode == contentFilter().adultTitles()) current = text;
  }
  // A dot says the mode is changed for now, away from the one in the settings
  const auto submenu = menu->addMenu(
      contentFilter().adultTitlesChanged() ? QObject::tr("Adult titles: %1 •").arg(current)
                                           : QObject::tr("Adult titles: %1").arg(current));
  const auto group = new QActionGroup(submenu);
  for (const auto& [text, mode] : modes) {
    // Applied once the menu is gone. Filtering the page right away kept the menu on screen with
    // the new mark drawn and the old one not yet cleared, and Hyprland's fade-out showed that
    // frame: two choices seemed to be checked.
    const auto action = submenu->addAction(text, submenu, [mode]() {
      QTimer::singleShot(0, &contentFilter(), [mode]() { contentFilter().setAdultTitles(mode); });
    });
    action->setCheckable(true);
    action->setChecked(contentFilter().adultTitles() == mode);
    group->addAction(action);
  }

  if (contentFilter().adultTitlesChanged()) {
    submenu->addSeparator();
    submenu->addAction(QObject::tr("Back to default"), submenu, []() {
      QTimer::singleShot(0, &contentFilter(), []() {
        contentFilter().setAdultTitles(contentFilter().defaultAdultTitles());
      });
    });
  }
}

}  // namespace gui
