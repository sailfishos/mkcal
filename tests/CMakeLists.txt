set(SRC_STORAGE
	tst_storage.cpp)
set(HEADERS_STORAGE
	tst_storage.h)

add_executable(tst_storage ${SRC_STORAGE} ${HEADERS_STORAGE})

target_include_directories(tst_storage PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(tst_storage
	Qt5::DBus
	Qt5::Test
	KF5::CalendarCore
	PkgConfig::SQLITE3
	PkgConfig::TIMED
	mkcal-qt5)

add_test(tst_storage tst_storage)
add_definitions(-DTIMED_SUPPORT)

set(SRC_PERF
	tst_perf.cpp)
set(HEADERS_PERF
	tst_perf.h)

add_executable(tst_perf ${SRC_PERF} ${HEADERS_PERF})

target_include_directories(tst_perf PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(tst_perf
	Qt5::Test
	KF5::CalendarCore
	mkcal-qt5)

add_test(tst_perf tst_perf)

add_executable(tst_load tst_load.cpp)

target_include_directories(tst_load PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(tst_load
	Qt5::Test
	KF5::CalendarCore
	mkcal-qt5)

add_test(tst_load tst_load)

add_executable(tst_backend tst_backend.cpp)

target_include_directories(tst_backend PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(tst_backend
	Qt5::Test
	KF5::CalendarCore
	mkcal-qt5)

add_test(tst_backend tst_backend)

add_executable(tst_sqliteformat tst_sqliteformat.cpp)

target_include_directories(tst_sqliteformat PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(tst_sqliteformat
	Qt5::Test
	KF5::CalendarCore
	PkgConfig::SQLITE3
	mkcal-qt5)

add_test(tst_sqliteformat tst_sqliteformat)

if(INSTALL_TESTS)
	install(TARGETS tst_storage
		DESTINATION /opt/tests/mkcal)
	install(TARGETS tst_load
		DESTINATION /opt/tests/mkcal)
	install(TARGETS tst_perf
		DESTINATION /opt/tests/mkcal)
	install(TARGETS tst_backend
		DESTINATION /opt/tests/mkcal)
	install(TARGETS tst_sqliteformat
		DESTINATION /opt/tests/mkcal)
	install(FILES tests.xml
		DESTINATION /opt/tests/mkcal)
endif()
