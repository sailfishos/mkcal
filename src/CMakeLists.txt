set(SRC
	extendedcalendar.cpp
	extendedstorage.cpp
	storagebackend.cpp
	notebook.cpp
	sqliteformat.cpp
	sqlitestorage.cpp
	servicehandler.cpp
	logging.cpp
	semaphore_p.cpp)
set(HEADERS
	extendedcalendar.h
	extendedstorage.h
	storagebackend.h
	notebook.h
	sqliteformat.h
	sqlitestorage.h
	servicehandlerif.h
	servicehandler.h
	dummystorage.h
	mkcal_export.h
	logging_p.h
	semaphore_p.h
	invitationhandlerif.h
	config-mkcal.h)

add_library(mkcal-qt5 SHARED ${SRC} ${HEADERS})

target_link_libraries(mkcal-qt5
		PRIVATE
	Qt5::DBus
	PkgConfig::SQLITE3
	PkgConfig::TIMED
		PUBLIC
	Qt5::Gui
	KF5::CalendarCore)

set_target_properties(mkcal-qt5 PROPERTIES
	SOVERSION ${PROJECT_VERSION_MAJOR}
	VERSION ${PROJECT_VERSION})

add_definitions(-fvisibility=hidden -fvisibility-inlines-hidden)
add_definitions(-DMKCALPLUGINDIR="${CMAKE_INSTALL_FULL_LIBDIR}/mkcalplugins")
add_definitions(-DTIMED_SUPPORT)

# Install the library
install(TARGETS mkcal-qt5
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install headers
install(FILES ${HEADERS}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mkcal-qt5
	COMPONENT Devel)

ecm_generate_pkgconfig_file(
	BASE_NAME libmkcal-qt5
	LIB_NAME mkcal-qt5
	INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR}/mkcal-qt5
	DESCRIPTION ${PROJECT_DESCRIPTION}
	INSTALL)
