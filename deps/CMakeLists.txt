add_library(akyuu-deps STATIC)

target_link_libraries(akyuu-deps
	anisthesia
	anitomy
	nstd
	semaver
	akyuu-config
	utf8proc
)

# utf8proc is linked statically, so its own install rules only get in the way: they would put
# utf8proc.h, libutf8proc.a and the pkg-config and CMake files next to Akyuu, over the files of
# the system package.
# `option()` there would override a plain variable: utf8proc still asks for CMake 3.10, so
# CMP0077 is OLD in its directory and the cache entry is what counts.
set(UTF8PROC_INSTALL OFF CACHE BOOL "Enable installation of utf8proc" FORCE)

add_subdirectory(anisthesia)
add_subdirectory(anitomy)
add_subdirectory(nstd)
add_subdirectory(semaver)
add_subdirectory(utf8proc)
