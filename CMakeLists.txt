cmake_minimum_required(VERSION 3.21)

# The version is kept in one place, `src/akyuu/config.h`, where the installer reads it as well.
# `tools/bump-version.sh` changes it there.
file(STRINGS src/akyuu/config.h _akyuu_version_lines REGEX "^#define AKYUU_VERSION_(MAJOR|MINOR|PATCH) ")
foreach(_line IN LISTS _akyuu_version_lines)
	string(REGEX MATCH "AKYUU_VERSION_([A-Z]+) +([0-9]+)" _ "${_line}")
	set(_akyuu_version_${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
endforeach()

project(Akyuu
	VERSION ${_akyuu_version_MAJOR}.${_akyuu_version_MINOR}.${_akyuu_version_PATCH}
	DESCRIPTION "Keeps the chronicle of the anime you watch"
	HOMEPAGE_URL "https://github.com/ligneo/akyuu"
	LANGUAGES CXX
)

option(AKYUU_BUILD_TESTS "Build focused Akyuu tests" OFF)
if (AKYUU_BUILD_TESTS)
	include(CTest)
endif()

set(AKYUU_PACKAGE_FORMAT "source" CACHE STRING "Distribution package format")
set_property(CACHE AKYUU_PACKAGE_FORMAT PROPERTY STRINGS source appimage arch deb rpm)
if (NOT AKYUU_PACKAGE_FORMAT MATCHES "^(source|appimage|arch|deb|rpm)$")
	message(FATAL_ERROR "Unsupported AKYUU_PACKAGE_FORMAT")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("#include <algorithm>
#include <array>
int main() { return std::ranges::starts_with(std::array{1, 2}, std::array{1}) ? 0 : 1; }"
	AKYUU_HAS_CXX23_RANGES)
if (NOT AKYUU_HAS_CXX23_RANGES)
	message(FATAL_ERROR "A C++23 standard library with std::ranges::starts_with is required (for example GCC 16 or MSVC 2022).")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	option(AKYUU_PORTABLE "Portable mode" ON)
else()
	option(AKYUU_PORTABLE "Portable mode" OFF)
endif()

include(AkyuuConfig)

add_subdirectory(deps)
add_subdirectory(src)
