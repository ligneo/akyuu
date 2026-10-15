cmake_minimum_required(VERSION 3.21)

# The version is kept in one place, `src/akyuu/config.h`, where the installer reads it as well.
# The release tool updates it there.
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
set_property(CACHE AKYUU_PACKAGE_FORMAT PROPERTY STRINGS source appimage arch deb rpm nix system)
if (NOT AKYUU_PACKAGE_FORMAT MATCHES "^(source|appimage|arch|deb|rpm|nix|system)$")
	message(FATAL_ERROR "Unsupported AKYUU_PACKAGE_FORMAT")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("#include <vector>
int main() { std::vector<int> v; v.append_range(std::vector{1}); return v.size() == 1 ? 0 : 1; }"
	AKYUU_HAS_CXX23_LIBRARY)
if (NOT AKYUU_HAS_CXX23_LIBRARY)
	message(FATAL_ERROR "A C++23 standard library with std::vector::append_range is required (for example GCC 15 or MSVC 2022).")
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
