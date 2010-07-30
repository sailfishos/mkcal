project(mkcal)

############### Build Options ###############

# The following macros can be defined to alter how mKCal is built.
# For example, cmake -DMKCAL_FOR_MEEGO=true

# MKCAL_FOR_MEEGO - builds mKCal with some special features for the MeeGo platform.
option(MKCAL_FOR_MEEGO "Build mKCal especially for the MeeGo." FALSE)

# MKCAL_TRACKER_SYNC - builds the mKCal Sqlite storage with synchronous tracking.
# By default, asynchronous tracking is used, but this doesn't provide error messages
# which might be troubling at some points; therefore, turn this on if you suspect
# something isn't being updated to tracker and want to hear why.
option(MKCAL_TRACKER_SYNC "Build mKCal especially for the MeeGo." FALSE)

# add C++ macro definitions for options passed to CMake
if(MKCAL_FOR_MEEGO)
  add_definitions(-DMKCAL_FOR_MEEGO)
endif(MKCAL_FOR_MEEGO)
if(MKCAL_TRACKER_SYNC)
  add_definitions(-DMKCAL_TRACKER_SYNC)
endif(MKCAL_TRACKER_SYNC)

############### search packages used by KDE ###############
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

set(QT_MIN_VERSION "4.6.0")
find_package(KDE4 4.4.0)
if(KDE4_FOUND)
  include(KDE4Defaults)
  include(MacroLibrary)
else(KDE4_FOUND)
  find_package(Qt4 ${QT_MIN_VERSION} COMPONENTS QtCore QtGui QtXml REQUIRED)
  find_package(Automoc4)
  include(CheckCXXCompilerFlag)
  include(MacroEnsureVersion)
  include(MacroLogFeature)
  include(KDE4Macros)

  set(BIN_INSTALL_DIR "bin")
  set(LIB_INSTALL_DIR "lib")
  set(GENERIC_LIB_VERSION "4.6.0")
  set(GENERIC_LIB_SOVERSION "4")
  set(INSTALL_TARGETS_DEFAULT_ARGS RUNTIME DESTINATION "${BIN_INSTALL_DIR}"
                                   LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
                                   ARCHIVE DESTINATION "${LIB_INSTALL_DIR}" COMPONENT Devel)
  # on the Mac support an extra install directory for application bundles starting with cmake 2.6
  if(APPLE)
    set(INSTALL_TARGETS_DEFAULT_ARGS ${INSTALL_TARGETS_DEFAULT_ARGS}
                                     BUNDLE DESTINATION "${BUNDLE_INSTALL_DIR}")
  endif(APPLE)

  set(__KDE_HAVE_GCC_VISIBILITY 0)
  if(CMAKE_COMPILER_IS_GNUCXX)

    # get the gcc version
    exec_program(${CMAKE_C_COMPILER} ARGS ${CMAKE_C_COMPILER_ARG1} --version OUTPUT_VARIABLE _gcc_version_info)

    string(REGEX MATCH "[345]\\.[0-9]\\.[0-9]" _gcc_version "${_gcc_version_info}")
    # gcc on mac just reports: "gcc (GCC) 3.3 20030304 ..." without the patch level, handle this here:
    if(NOT _gcc_version)
      string(REGEX MATCH ".*\\(GCC\\).* ([34]\\.[0-9]) .*" "\\1.0" _gcc_version "${gcc_on_macos}")
      if(gcc_on_macos)
        string(REGEX REPLACE ".*\\(GCC\\).* ([34]\\.[0-9]) .*" "\\1.0" _gcc_version "${_gcc_version_info}")
      endif(gcc_on_macos)
    endif(NOT _gcc_version)

    if(_gcc_version)
      macro_ensure_version("4.1.0" "${_gcc_version}" GCC_IS_NEWER_THAN_4_1)
      macro_ensure_version("4.2.0" "${_gcc_version}" GCC_IS_NEWER_THAN_4_2)
      macro_ensure_version("4.3.0" "${_gcc_version}" GCC_IS_NEWER_THAN_4_3)
    endif(_gcc_version)

    check_cxx_compiler_flag(-fvisibility=hidden __KDE_HAVE_GCC_VISIBILITY)
    set(__KDE_HAVE_GCC_VISIBILITY ${__KDE_HAVE_GCC_VISIBILITY} CACHE BOOL "GCC support for hidden visibility")

    set(_GCC_COMPILED_WITH_BAD_ALLOCATOR FALSE)
    if(GCC_IS_NEWER_THAN_4_1)
      exec_program(${CMAKE_C_COMPILER} ARGS ${CMAKE_C_COMPILER_ARG1} -v OUTPUT_VARIABLE _gcc_alloc_info)
      string(REGEX MATCH "(--enable-libstdcxx-allocator=mt)" _GCC_COMPILED_WITH_BAD_ALLOCATOR "${_gcc_alloc_info}")
    endif (GCC_IS_NEWER_THAN_4_1)

    if(__KDE_HAVE_GCC_VISIBILITY AND GCC_IS_NEWER_THAN_4_1 AND NOT _GCC_COMPILED_WITH_BAD_ALLOCATOR AND NOT WIN32)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
      if(GCC_IS_NEWER_THAN_4_2)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
      endif(GCC_IS_NEWER_THAN_4_2)
    else(__KDE_HAVE_GCC_VISIBILITY AND GCC_IS_NEWER_THAN_4_1 AND NOT _GCC_COMPILED_WITH_BAD_ALLOCATOR AND NOT WIN32)
      set(__KDE_HAVE_GCC_VISIBILITY 0)
    endif(__KDE_HAVE_GCC_VISIBILITY AND GCC_IS_NEWER_THAN_4_1 AND NOT _GCC_COMPILED_WITH_BAD_ALLOCATOR AND NOT WIN32)
  endif(CMAKE_COMPILER_IS_GNUCXX)

  configure_file(kdelibs-copy/kdemacros.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdemacros.h)
endif(KDE4_FOUND)

############### Needed commands before building anything ###############

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

############### search for external packages ##############

set(SQLITE_MIN_VERSION "3.6.23")
find_package(Sqlite)
macro_log_feature(SQLITE_FOUND "Sqlite" "SQlite database library" "http://www.sqlite.org" FALSE "${SQLITE_MIN_VERSION}" "Necessary to build the SQlite storage backend.")

###########################################################

#TODO: reserve a proper debug area for this library and use it
add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5802)

include(ConfigureChecks.cmake)
configure_file(config-mkcal.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-mkcal.h)

if(KDE4_BUILD_TESTS)
  add_definitions(-DCOMPILING_TESTS)
endif(KDE4_BUILD_TESTS)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/qtlockedfile/src
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${KDE4_INCLUDE_DIR}
  ${SQLITE_INCLUDE_DIR}
)

########### next target ###############

#KDAB_TODO: we cannot require kdelibs. make it optional. if not found, use our own local copy
set(mkcal_kdelibs_copy_LIB_SRCS
)

if(SQLITE_FOUND)
  set(sqlite_SRCS sqliteformat.cpp sqlitestorage.cpp)
endif(SQLITE_FOUND)

if(WIN32)
  set(qtlockfile_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/qtlockedfile/src/qtlockedfile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qtlockedfile/src/qtlockedfile_win.cpp)
else(WIN32)
  set(qtlockfile_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/qtlockedfile/src/qtlockedfile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qtlockedfile/src/qtlockedfile_unix.cpp)
endif(WIN32)


set(mkcal_LIB_SRCS
  directorystorage.cpp
  extendedcalendar.cpp
  extendedstorage.cpp
  invitationhandler.cpp
  notebook.cpp
  ${qtlockfile_SRCS}
  ${sqlite_SRCS}
  trackerformat.cpp
  trackermodify.cpp
  trackerstorage.cpp
)


kde4_add_library(mkcal SHARED ${mkcal_LIB_SRCS})

target_link_libraries(mkcal ${KDE4_KDECORE_LIBRARY} kcalcore ${SQLITE_LIBRARIES} ${QT_QTGUI_LIBRARY})
if(HAVE_UUID_UUID_H)
  target_link_libraries(mkcal uuid)
endif(HAVE_UUID_UUID_H)
set_target_properties(mkcal PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION})
install(TARGETS mkcal EXPORT kdepimlibsLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

########### next target ###############

#add_subdirectory(tests)

########### install files ###############

install(FILES
  directorystorage.h
  extendedcalendar.h
  extendedstorage.h
  invitationhandler.h
  invitationhandlerif.h
  mkcal_export.h
  notebook.h
  sqliteformat.h
  sqlitestorage.h
  trackerformat.h
  trackermodify.h
  trackerstorage.h
  DESTINATION ${INCLUDE_INSTALL_DIR}/mkcal COMPONENT Devel)

macro_display_feature_log()
