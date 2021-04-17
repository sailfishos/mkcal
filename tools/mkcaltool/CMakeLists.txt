set(SRC
	main.cpp
	mkcaltool.cpp)
set(HEADERS
	mkcaltool.h)

add_executable(mkcaltool ${SRC} ${HEADERS})

target_include_directories(mkcaltool PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(mkcaltool
	KF5::CalendarCore
	mkcal-qt5)

install(TARGETS mkcaltool
	DESTINATION ${CMAKE_INSTALL_BINDIR})
