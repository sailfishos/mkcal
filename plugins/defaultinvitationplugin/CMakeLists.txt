set(SRC
	defaultinvitationplugin.cpp)
set(HEADERS
	defaultinvitationplugin.h)

add_library(defaultinvitationplugin ${SRC} ${HEADERS})

target_include_directories(defaultinvitationplugin PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(defaultinvitationplugin
	KF5::CalendarCore
	mkcal-qt5
	PkgConfig::QMF)

install(TARGETS defaultinvitationplugin
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/mkcalplugins)
