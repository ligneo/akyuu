include(QtSetup)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(akyuu)

target_sources(akyuu PRIVATE
	base/atf.cpp
	base/atf.hpp
	base/chrono.cpp
	base/chrono.hpp
	base/file.cpp
	base/file.hpp
	base/log.cpp
	base/log.hpp
	base/preprocessor.h
	base/rss.cpp
	base/rss.hpp
	base/settings.cpp
	base/settings.hpp
	base/string.cpp
	base/string.hpp
	base/xml.cpp
	base/xml.hpp

	compat/anime.cpp
	compat/anime.hpp
	compat/common.cpp
	compat/common.hpp
	compat/history.cpp
	compat/history.hpp
	compat/list.cpp
	compat/list.hpp
	compat/settings.cpp
	compat/settings.hpp

	link/discord.cpp
	link/http.cpp
	link/http.hpp
	link/discord.hpp

	media/anime_db.cpp
	media/anime_db.hpp
	media/anime_history.cpp
	media/anime_history.hpp
	media/anime_list_export.cpp
	media/anime_list_export.hpp
	media/anime_list_utils.cpp
	media/anime_list_utils.hpp
	media/anime_list.hpp
	media/anime_season.cpp
	media/anime_season.hpp
	media/anime_settings.hpp
	media/anime_stats.cpp
	media/anime_stats.hpp
	media/anime_utils.cpp
	media/anime_utils.hpp
	media/anime.hpp

	sync/anilist/anilist.cpp
	sync/anilist/anilist.hpp
	sync/anilist/anilist_auth.cpp
	sync/anilist/anilist_error.cpp
	sync/anilist/anilist_error.hpp
	sync/anilist/anilist_parsers.cpp
	sync/anilist/anilist_parsers.hpp
	sync/anilist/anilist_ratings.cpp
	sync/anilist/anilist_ratings.hpp
	sync/anilist/anilist_utils.cpp
	sync/anilist/anilist_utils.hpp
	sync/kitsu/kitsu.cpp
	sync/kitsu/kitsu.hpp
	sync/kitsu/kitsu_auth.cpp
	sync/kitsu/kitsu_error.cpp
	sync/kitsu/kitsu_error.hpp
	sync/kitsu/kitsu_parsers.cpp
	sync/kitsu/kitsu_parsers.hpp
	sync/kitsu/kitsu_ratings.cpp
	sync/kitsu/kitsu_ratings.hpp
	sync/kitsu/kitsu_utils.cpp
	sync/kitsu/kitsu_utils.hpp
	sync/myanimelist/myanimelist.cpp
	sync/myanimelist/myanimelist.hpp
	sync/myanimelist/myanimelist_auth.cpp
	sync/myanimelist/myanimelist_error.cpp
	sync/myanimelist/myanimelist_error.hpp
	sync/myanimelist/myanimelist_parsers.cpp
	sync/myanimelist/myanimelist_parsers.hpp
	sync/myanimelist/myanimelist_ratings.cpp
	sync/myanimelist/myanimelist_ratings.hpp
	sync/myanimelist/myanimelist_utils.cpp
	sync/myanimelist/myanimelist_utils.hpp
	sync/queue.cpp
	sync/queue.hpp
	sync/service.cpp
	sync/service.hpp

	akyuu/accounts.cpp
	akyuu/autostart.cpp
	akyuu/autostart.hpp
	akyuu/accounts.hpp
	akyuu/application.cpp
	akyuu/application.hpp
	akyuu/config.h
	akyuu/network.cpp
	akyuu/network.hpp
	akyuu/hoot.cpp
	akyuu/hoot.hpp
	akyuu/path.cpp
	akyuu/path.hpp
	akyuu/release_selection.cpp
	akyuu/release_selection.hpp
	akyuu/session.cpp
	akyuu/session.hpp
	akyuu/script.cpp
	akyuu/script.hpp
	akyuu/settings.cpp
	akyuu/settings.hpp
	akyuu/version.cpp
	akyuu/version.hpp

	track/episode.cpp
	track/library.cpp
	track/library.hpp
	track/feed.cpp
	track/feed_aggregator.cpp
	track/feed_aggregator.hpp
	track/feed_archive.cpp
	track/feed_archive.hpp
	track/feed_filter.cpp
	track/feed_filter.hpp
	track/feed_filter_manager.cpp
	track/feed_filter_manager.hpp
	track/feed_filter_util.cpp
	track/feed_filter_util.hpp
	track/feed.hpp
	track/episode.hpp
	track/media.cpp
	track/media.hpp
	track/media_player.cpp
	track/media_player.hpp
	track/media_stream.cpp
	track/media_stream.hpp
	track/play.cpp
	track/play.hpp
	track/recognition.cpp
	track/recognition.hpp
	track/recognition_cache.cpp
	track/recognition_cache.hpp
	track/recognition_normalize.cpp
	track/recognition_normalize.hpp
	track/recognition_path.cpp
	track/recognition_path.hpp
	track/recognition_relations.cpp
	track/recognition_relations.hpp
	track/recognition_validate.cpp
	track/recognition_validate.hpp
	track/scanner.cpp
	track/scanner.hpp
	track/update_decision.cpp
	track/update_decision.hpp
	track/update_session.cpp
	track/update_session.hpp
	track/update_state.hpp
	track/update_trigger.hpp

	main.cpp
)

target_link_libraries(akyuu PRIVATE
	Qt6::Core
	Qt6::Gui
	Qt6::Network
	Qt6::Sql
	Qt6::Widgets
	akyuu-config
	akyuu-deps
	akyuu-gui
	akyuu-resources
)

if (AKYUU_PORTABLE)
	target_compile_definitions(akyuu PRIVATE AKYUU_PORTABLE)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set_target_properties(akyuu PROPERTIES OUTPUT_NAME Akyuu)
	set_target_properties(akyuu PROPERTIES WIN32_EXECUTABLE ON)
endif()

# The Hoot easter egg needs a way to play its notes; Windows has `Beep`, everywhere else it
# takes an audio sink. Qt Multimedia pulls in a whole media backend, so it stays optional: without
# it the easter egg is silent and nothing else changes.
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	find_package(Qt6 QUIET COMPONENTS Multimedia)
	if (Qt6Multimedia_FOUND)
		target_link_libraries(akyuu PRIVATE Qt6::Multimedia)
		target_compile_definitions(akyuu PRIVATE AKYUU_HAS_MULTIMEDIA)
	endif()
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	find_package(Qt6 REQUIRED COMPONENTS DBus)
	target_sources(akyuu PRIVATE
		link/irc.cpp
		link/irc.hpp
		track/media_focus.cpp
		track/media_focus.hpp
	)
	target_link_libraries(akyuu PRIVATE Qt6::DBus)

	include(GNUInstallDirs)
	install(TARGETS akyuu RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

add_subdirectory(gui)
add_subdirectory(resources)

qt_add_translations(akyuu
	SOURCE_TARGETS akyuu-gui
	TS_FILE_BASE akyuu
	TS_FILE_DIR resources/translations
	LUPDATE_OPTIONS -no-obsolete -locations none
)

if (AKYUU_BUILD_TESTS AND BUILD_TESTING)
	add_executable(akyuu-release-selection-tests
		${CMAKE_SOURCE_DIR}/tests/release_selection/release_selection.cpp
		akyuu/release_selection.cpp
	)
	set_target_properties(akyuu-release-selection-tests PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
	)
	target_link_libraries(akyuu-release-selection-tests PRIVATE Qt6::Core semaver)
	add_test(NAME release-selection COMMAND akyuu-release-selection-tests)

	add_executable(akyuu-private-settings-tests
		${CMAKE_SOURCE_DIR}/tests/private_settings.cpp
		base/settings.cpp
	)
	set_target_properties(akyuu-private-settings-tests PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
	)
	target_include_directories(akyuu-private-settings-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(akyuu-private-settings-tests PRIVATE Qt6::Core)
	add_test(NAME private-settings COMMAND akyuu-private-settings-tests)
	add_executable(akyuu-streaming-tests
		${CMAKE_SOURCE_DIR}/tests/streaming/streaming.cpp
		track/media_stream.cpp
	)
	set_target_properties(akyuu-streaming-tests PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
	)
	target_include_directories(akyuu-streaming-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(akyuu-streaming-tests PRIVATE Qt6::Core Qt6::Network anitomy)
	add_test(NAME streaming COMMAND akyuu-streaming-tests)
endif()
