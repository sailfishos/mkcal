set(SRC
	main.cpp
	mkcaltool.cpp)
set(HEADERS
	mkcaltool.h)

add_executable(mkcaltool ${SRC} ${HEADERS})

target_include_directories(mkcaltool PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(mkcaltool
	KF${QT_VERSION_MAJOR}::CalendarCore
	mkcal-qt${QT_VERSION_MAJOR})

install(TARGETS mkcaltool
	DESTINATION ${CMAKE_INSTALL_BINDIR})
