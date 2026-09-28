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

#pragma once

#include <QApplication>
#include <QObject>
#include <QTimer>
#include <anisthesia.hpp>
#include <optional>
#include <string>
#include <vector>

#include "track/episode.hpp"

namespace track::media {

class Detection final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(Detection)

public:
  using media_t = anisthesia::Media;
  using player_t = anisthesia::Player;
  using player_id_t = anisthesia::PlayerId;

  Detection(QObject* parent);

  const std::optional<Episode> getCurrentEpisode() const;
  const std::optional<std::string> getCurrentFile() const;
  const std::optional<media_t> getCurrentMedia() const;
  const std::optional<player_t> getCurrentPlayer() const;

  bool init();
  void setEnabled(const bool enabled);

  bool isMediaIdentified() const;
  bool isPlayerFocused() const;
  void setCurrentEpisodeAnimeId(int animeId);

signals:
  void enabledChanged(bool enabled) const;
  void currentEpisodeChanged(std::optional<Episode> media) const;

private:
  void poll();
  void reset();

  bool hasEpisodeChanged(const Episode& episode) const;

  std::optional<Episode> currentEpisode_;
  std::optional<media_t> currentMedia_;
  std::optional<player_t> currentPlayer_;
  player_id_t currentPlayerId_ = {};
  std::vector<player_t> players_;

  QTimer* pollTimer_;
};

inline Detection* detection() {
  static auto detection = new Detection(qApp);
  return detection;
}

}  // namespace track::media
