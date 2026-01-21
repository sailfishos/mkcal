cmake_minimum_required(VERSION 3.16)

project(mkcal
	VERSION 0.7.24
	DESCRIPTION "Mkcal calendar library")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(FeatureSummary)
include(GNUInstallDirs)
include(ECMGeneratePkgConfigFile)

option(BUILD_PLUGINS "Build plugin" ON)
option(BUILD_TESTS "Build tests" ON)
option(INSTALL_TESTS "Install the tests to the system" OFF)
option(BUILD_DOCUMENTATION "Build documentation" OFF)

find_package(PkgConfig REQUIRED)

set(QT_MIN_VERSION "5.6.0")
find_package(Qt5 ${QT_MIN_VERSION} COMPONENTS DBus Gui Test REQUIRED)
find_package(KF5 COMPONENTS CalendarCore REQUIRED)

pkg_check_modules(TIMED timed-qt5 IMPORTED_TARGET REQUIRED)
set_property(GLOBAL APPEND PROPERTY _CMAKE_timed-qt5_TYPE REQUIRED)
pkg_check_modules(SQLITE3 sqlite3 IMPORTED_TARGET REQUIRED)
set_property(GLOBAL APPEND PROPERTY _CMAKE_sqlite3_TYPE REQUIRED)

if(TIMED_FOUND)
	set_property(GLOBAL APPEND PROPERTY PACKAGES_FOUND timed-qt5)
else()
	set_property(GLOBAL APPEND PROPERTY PACKAGES_NOT_FOUND timed-qt5)
endif()
if(SQLITE3_FOUND)
	set_property(GLOBAL APPEND PROPERTY PACKAGES_FOUND sqlite3)
else()
	set_property(GLOBAL APPEND PROPERTY PACKAGES_NOT_FOUND sqlite3)
endif()

add_subdirectory(src)
add_subdirectory(tools)

if(BUILD_PLUGINS)
	pkg_check_modules(QMF QmfClient IMPORTED_TARGET REQUIRED)
	set_property(GLOBAL APPEND PROPERTY _CMAKE_QmfClient_TYPE REQUIRED)

	if(QMF_FOUND)
		set_property(GLOBAL APPEND PROPERTY PACKAGES_FOUND QmfClient)
	else()
		set_property(GLOBAL APPEND PROPERTY PACKAGES_NOT_FOUND QmfClient)
	endif()

	add_subdirectory(plugins)
endif()

if(BUILD_TESTS)
	include(CTest)
	add_subdirectory(tests)
endif()

if(BUILD_DOCUMENTATION)
	find_package(Doxygen REQUIRED)
	if(DOXYGEN_FOUND)	
		add_subdirectory(doc)
	endif()
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
