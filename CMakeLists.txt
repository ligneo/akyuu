cmake_minimum_required(VERSION 3.21)

project(Akyuu
	VERSION 0.1.0
	DESCRIPTION "Keeps the chronicle of the anime you watch"
	HOMEPAGE_URL "https://github.com/ligneo/akyuu"
	LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	option(AKYUU_PORTABLE "Portable mode" ON)
else()
	option(AKYUU_PORTABLE "Portable mode" OFF)
endif()

include(AkyuuConfig)

add_subdirectory(deps)
add_subdirectory(src)
