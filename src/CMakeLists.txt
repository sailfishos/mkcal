set(SRC
	extendedcalendar.cpp
	extendedstorage.cpp
	notebook.cpp
	sqliteformat.cpp
	sqlitestorage.cpp
	servicehandler.cpp
        alarmhandler.cpp
	logging.cpp
	semaphore_p.cpp)
set(HEADERS
	extendedcalendar.h
	extendedstorage.h
	extendedstorageobserver.h
	notebook.h
	sqlitestorage.h
	servicehandlerif.h
	servicehandler.h
	dummystorage.h
	mkcal_export.h
	invitationhandlerif.h)

set(PRIVATE_HEADERS
        alarmhandler_p.h
        logging_p.h
        semaphore_p.h
        sqliteformat.h
        )

set(MKCAL_NAME mkcal-qt${QT_VERSION_MAJOR})
add_library(${MKCAL_NAME} SHARED ${SRC} ${HEADERS} ${PRIVATE_HEADERS})

target_compile_definitions(${MKCAL_NAME} PRIVATE MKCAL_LIBRARY)
target_link_libraries(${MKCAL_NAME}
		PRIVATE
	PkgConfig::SQLITE3
		PUBLIC
	KF${QT_VERSION_MAJOR}::CalendarCore)

set_target_properties(${MKCAL_NAME} PROPERTIES
	SOVERSION ${PROJECT_VERSION_MAJOR}
	VERSION ${PROJECT_VERSION})

add_definitions(-fvisibility=hidden -fvisibility-inlines-hidden)
add_definitions(-DMKCALPLUGINDIR="${CMAKE_INSTALL_FULL_LIBDIR}/mkcalplugins")

if(TIMED_FOUND)
    target_link_libraries(${MKCAL_NAME}
        PRIVATE
            Qt${QT_VERSION_MAJOR}::DBus
            PkgConfig::TIMED
    )
    add_definitions(-DTIMED_SUPPORT)
endif()

# Install the library
install(TARGETS ${MKCAL_NAME}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install headers
install(FILES ${HEADERS}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${MKCAL_NAME}
	COMPONENT Devel)

ecm_generate_pkgconfig_file(
	BASE_NAME lib${MKCAL_NAME}
	LIB_NAME ${MKCAL_NAME}
	INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR}/${MKCAL_NAME}
	DESCRIPTION ${PROJECT_DESCRIPTION}
	INSTALL)
