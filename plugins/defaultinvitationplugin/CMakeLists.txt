set(SRC
	defaultinvitationplugin.cpp)
set(HEADERS
	defaultinvitationplugin.h)

add_library(defaultinvitationplugin ${SRC} ${HEADERS})

target_include_directories(defaultinvitationplugin PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(defaultinvitationplugin
	KF${QT_VERSION_MAJOR}::CalendarCore
	mkcal-qt${QT_VERSION_MAJOR}
	PkgConfig::QMF)

install(TARGETS defaultinvitationplugin
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/mkcalplugins)
