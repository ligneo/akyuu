/**
 * Akyuu
 * Copyright (C) 2010-2024, Eren Okka
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

// @NOTE: Make sure to update the following files after changing the names of
// macros defined here:
//
// - /setup/Akyuu.nsi
// - /src/resources/akyuu.rc

// Relative path to avoid RC1015 error with version.rc
#include "../base/preprocessor.h"

#define AKYUU_APP_NAME  "Akyuu"
#define AKYUU_APP_MUTEX "Akyuu-c7648072-cea2-4447-8df7-27b282acdd7e"

#define AKYUU_VERSION_MAJOR 0
#define AKYUU_VERSION_MINOR 1
#define AKYUU_VERSION_PATCH 0
#define AKYUU_VERSION_PRE   "beta.1"
#define AKYUU_VERSION_BUILD 0

// Used in akyuu.rc
#define AKYUU_VERSION_DIGITAL \
    AKYUU_VERSION_MAJOR, \
    AKYUU_VERSION_MINOR, \
    AKYUU_VERSION_PATCH, \
    AKYUU_VERSION_BUILD
#define AKYUU_VERSION_STRING \
    STRINGIZE(AKYUU_VERSION_MAJOR) "." \
    STRINGIZE(AKYUU_VERSION_MINOR) "." \
    STRINGIZE(AKYUU_VERSION_PATCH) "\0"
