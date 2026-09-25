add_library(akyuu-gui STATIC)

qt_add_ui(akyuu-gui SOURCES
	main/main_window.ui
	media/media_dialog.ui
	settings/settings_dialog.ui
)

target_sources(akyuu-gui PRIVATE
	common/anime_list_item_delegate.cpp
	common/anime_list_item_delegate.hpp
	common/anime_list_item_delegate_cards.cpp
	common/anime_list_item_delegate_cards.hpp
	common/anime_list_view.cpp
	common/anime_list_view.hpp
	common/anime_list_view_base.cpp
	common/anime_list_view_base.hpp
	common/anime_list_view_cards.cpp
	common/anime_list_view_cards.hpp
	common/clickable_label.cpp
	common/clickable_label.hpp
	common/combobox.cpp
	common/combobox.hpp
	common/page_widget.cpp
	common/page_widget.hpp
	common/poster_widget.cpp
	common/poster_widget.hpp
	common/spinner_widget.cpp
	common/spinner_widget.hpp

	history/history_widget.cpp
	history/history_widget.hpp

	library/library_menu.cpp
	library/library_menu.hpp
	library/library_widget.cpp
	library/library_widget.hpp

	list/list_widget.cpp
	list/list_widget.hpp

	main/about_dialog.cpp
	main/about_dialog.hpp
	main/main_window.cpp
	main/main_window.hpp
	main/navigation_controller.cpp
	main/navigation_controller.hpp
	main/navigation_item_delegate.cpp
	main/navigation_item_delegate.hpp
	main/navigation_widget.cpp
	main/navigation_widget.hpp
	main/now_playing_page.cpp
	main/now_playing_page.hpp
	main/now_playing_widget.cpp
	main/now_playing_widget.hpp
	main/status_bar.cpp
	main/status_bar.hpp
	main/status_bar_controller.cpp
	main/status_bar_controller.hpp

	media/media_dialog.cpp
	media/media_dialog.hpp
	media/media_menu.cpp
	media/media_menu.hpp

	models/anime_list_model.cpp
	models/anime_list_model.hpp
	models/anime_list_proxy_model.cpp
	models/anime_list_proxy_model.hpp
	models/history_model.cpp
	models/history_model.hpp
	models/history_proxy_model.cpp
	models/history_proxy_model.hpp
	models/library_model.cpp
	models/library_model.hpp

	profile/profile_widget.cpp
	profile/profile_widget.hpp

	seasons/seasons_widget.cpp
	seasons/seasons_widget.hpp
	search/search_widget.cpp
	search/search_widget.hpp

	models/torrent_model.cpp
	models/torrent_model.hpp
	torrents/filter_condition_dialog.cpp
	torrents/filter_condition_dialog.hpp
	torrents/filter_dialog.cpp
	torrents/filter_dialog.hpp
	torrents/torrents_widget.cpp
	torrents/torrents_widget.hpp
	settings/settings_accounts_page.cpp
	settings/settings_accounts_page.hpp
	settings/settings_anime_list_page.cpp
	settings/settings_anime_list_page.hpp
	settings/settings_application_page.cpp
	settings/settings_application_page.hpp
	settings/settings_discord_page.cpp
	settings/settings_http_page.cpp
	settings/settings_http_page.hpp
	settings/settings_discord_page.hpp
	settings/settings_dialog.cpp
	settings/settings_dialog.hpp
	settings/settings_library_page.cpp
	settings/settings_library_page.hpp
	settings/settings_media_players_page.cpp
	settings/settings_media_players_page.hpp
	settings/settings_page.hpp
	settings/settings_player_list.cpp
	settings/settings_player_list.hpp
	settings/settings_advanced_page.cpp
	settings/settings_advanced_page.hpp
	settings/settings_cache_page.cpp
	settings/settings_cache_page.hpp
	settings/settings_recognition_page.cpp
	settings/settings_recognition_page.hpp
	settings/settings_stream_list.cpp
	settings/settings_stream_list.hpp
	settings/settings_torrent_downloads_page.cpp
	settings/settings_torrent_downloads_page.hpp
	settings/settings_torrent_filters_page.cpp
	settings/settings_torrent_filters_page.hpp
	settings/settings_torrents_page.cpp
	settings/settings_torrents_page.hpp
	settings/settings_streaming_page.cpp
	settings/settings_streaming_page.hpp

	utils/format.cpp
	utils/format.hpp
	utils/image_provider.cpp
	utils/image_provider.hpp
	utils/painter_state_saver.hpp
	utils/painters.cpp
	utils/painters.hpp
	utils/rating.cpp
	utils/rating.hpp
	utils/svg_icon_engine.cpp
	utils/svg_icon_engine.hpp
	utils/theme.cpp
	utils/theme.hpp
	utils/tray_icon.cpp
	utils/tray_icon.hpp
	utils/widgets.cpp
	utils/widgets.hpp
)

target_link_libraries(akyuu-gui
	Qt6::Concurrent
	Qt6::Core
	Qt6::Gui
	Qt6::Network
	Qt6::Sql
	Qt6::Widgets
	akyuu-config
	akyuu-deps
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_sources(akyuu-gui PRIVATE platforms/windows.cpp)
	target_link_libraries(akyuu-gui Dwmapi.lib)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_sources(akyuu-gui PRIVATE
		settings/settings_irc_page.cpp
		settings/settings_irc_page.hpp
	)
endif()
