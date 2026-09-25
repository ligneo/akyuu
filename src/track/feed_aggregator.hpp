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

#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QRestAccessManager>
#include <QStringList>
#include <QTimer>
#include <chrono>

#include "akyuu/network.hpp"
#include "track/feed.hpp"

namespace track {

class Aggregator final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(Aggregator)

public:
  Aggregator(QObject* parent);
  ~Aggregator() override = default;

  void fetch(const QString& url = {}, const bool automatic = false);
  void download(const FeedItem& item);
  void downloadSelected();
  void discardSelected();
  void search(const QString& title);

  void applyAutoCheckSettings();
  std::chrono::milliseconds timeUntilNextCheck() const;

  QStringList newEpisodeLines() const;

  bool fetching() const;
  const Feed& feed() const;

signals:
  void feedChanged();
  void newEpisodesFound(const QStringList& lines);
  void fetchingChanged(bool fetching);
  void errorOccurred(const QString& message);
  void downloadFinished(const QString& title);

private:
  QRestAccessManager manager_{akyuu::network()};
  QTimer timer_{this};
  Feed feed_;
  bool fetching_ = false;
};

inline Aggregator* aggregator() {
  static auto instance = new Aggregator(qApp);
  return instance;
}

}  // namespace track
