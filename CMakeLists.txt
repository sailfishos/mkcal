cmake_minimum_required(VERSION 3.16)

project(mkcal
	VERSION 0.7.22
	DESCRIPTION "Mkcal calendar library")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(FeatureSummary)
include(GNUInstallDirs)
include(ECMGeneratePkgConfigFile)

option(BUILD_PLUGINS "Build plugin" ON)
option(BUILD_TESTS "Build tests" ON)
option(INSTALL_TESTS "Install the tests to the system" OFF)
option(BUILD_DOCUMENTATION "Build documentation" OFF)
option(ENABLE_QT6 "Build against Qt 6" OFF)

find_package(PkgConfig REQUIRED)

if(ENABLE_QT6)
    set(QT_MIN_VERSION "6.5.0")
    set(QT_VERSION_MAJOR 6)
    find_package(Qt6 ${QT_MIN_VERSION} COMPONENTS Core Test REQUIRED)
    find_package(KF6 COMPONENTS CalendarCore REQUIRED)

    set(QMFCLIENT_NAME QmfClient-qt6)
else()
    set(QT_MIN_VERSION "5.6.0")
    set(QT_VERSION_MAJOR 5)
    find_package(Qt5 ${QT_MIN_VERSION} COMPONENTS Core Test REQUIRED)
    find_package(KF5 COMPONENTS CalendarCore REQUIRED)

    set(QMFCLIENT_NAME QmfClient)
endif()

pkg_check_modules(SQLITE3 sqlite3 IMPORTED_TARGET REQUIRED)
set_property(GLOBAL APPEND PROPERTY _CMAKE_sqlite3_TYPE REQUIRED)

pkg_check_modules(QMF ${QMFCLIENT_NAME} IMPORTED_TARGET)
if(QMF_FOUND)
    set_property(GLOBAL APPEND PROPERTY PACKAGES_FOUND ${QMFCLIENT_NAME})
else()
    set_property(GLOBAL APPEND PROPERTY PACKAGES_NOT_FOUND ${QMFCLIENT_NAME})
endif()

pkg_check_modules(TIMED timed-qt${QT_VERSION_MAJOR} IMPORTED_TARGET)
if(TIMED_FOUND)
    set_property(GLOBAL APPEND PROPERTY PACKAGES_FOUND timed-qt${QT_VERSION_MAJOR})

    find_package(Qt${QT_VERSION_MAJOR} ${QT_MIN_VERSION} COMPONENTS DBus REQUIRED)
else()
    set_property(GLOBAL APPEND PROPERTY PACKAGES_NOT_FOUND timed-qt${QT_VERSION_MAJOR})
endif()

if(SQLITE3_FOUND)
    set_property(GLOBAL APPEND PROPERTY PACKAGES_FOUND sqlite3)
else()
    set_property(GLOBAL APPEND PROPERTY PACKAGES_NOT_FOUND sqlite3)
endif()

add_subdirectory(src)
add_subdirectory(tools)

if(BUILD_PLUGINS)
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
