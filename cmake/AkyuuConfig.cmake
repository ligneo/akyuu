add_library(akyuu-config INTERFACE)

target_compile_definitions(akyuu-config INTERFACE
	QT_DISABLE_DEPRECATED_UP_TO=0x060A00
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_definitions(akyuu-config INTERFACE
		_WIN32_WINNT=0x0A00
		WIN32_LEAN_AND_MEAN
		NOMINMAX
		_UNICODE
		UNICODE
	)
endif()

if (MSVC)
	target_compile_options(akyuu-config INTERFACE
		/guard:cf
		/MP
		/permissive-
		/utf-8
		/W3
		/Zc:__cplusplus
	)
else()
	target_compile_options(akyuu-config INTERFACE
		-Wall
		-Wextra
	)
endif()
