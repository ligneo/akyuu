add_library(akyuu-resources STATIC)

target_sources(akyuu-resources PUBLIC
	deps.qrc
	gql.qrc
	icons.qrc
	styles.qrc
	sql.qrc
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_sources(akyuu PRIVATE
		akyuu.manifest
		akyuu.rc
	)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	include(GNUInstallDirs)
	install(FILES moe.taiga.Taiga.desktop
		DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
	)
	install(FILES icons/taiga.png
		DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
		RENAME moe.taiga.Taiga.png
	)
endif()

target_link_libraries(akyuu-resources
	akyuu-config
)
