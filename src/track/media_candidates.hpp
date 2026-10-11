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

#include <anisthesia.hpp>
#include <optional>
#include <vector>

namespace track::media {

struct Candidate {
  const anisthesia::Result* result = nullptr;
  anisthesia::Media media;
};

// Each media session (e.g. a browser tab reported via MPRIS) becomes a candidate of its own, so
// that the title of one is never paired with the address of another. Single pieces of information
// (window titles, open files, UI Automation values) describe the same media and are merged.
std::vector<anisthesia::Media> splitMedia(const anisthesia::Result& result);

bool isPaused(const anisthesia::Media& media);

std::vector<Candidate> orderCandidates(const std::vector<anisthesia::Result>& results,
                                       const anisthesia::PlayerId& currentPlayerId,
                                       const std::optional<anisthesia::Media>& currentMedia);

}  // namespace track::media
