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

if(INSTALL_TESTS)
	install(TARGETS tst_storage
		DESTINATION /opt/tests/mkcal)
	install(TARGETS tst_perf
		DESTINATION /opt/tests/mkcal)
	install(FILES tests.xml
		DESTINATION /opt/tests/mkcal)
endif()
