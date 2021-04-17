set(SRC
	tst_storage.cpp)
set(HEADERS
	tst_storage.h)

add_executable(tst_storage ${SRC} ${HEADERS})

target_include_directories(tst_storage PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(tst_storage
	Qt5::DBus
	Qt5::Test
	KF5::CalendarCore
	PkgConfig::SQLITE3
	PkgConfig::TIMED
	mkcal-qt5)

add_test(tst_storage tst_storage)

if(INSTALL_TESTS)
	install(TARGETS tst_storage
		DESTINATION /opt/tests/mkcal)
	install(FILES tests.xml
		DESTINATION /opt/tests/mkcal)
endif()
