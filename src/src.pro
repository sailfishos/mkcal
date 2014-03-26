# QMake project file for mkcal sources.
# make coverage
coverage.CONFIG += recursive
QMAKE_EXTRA_TARGETS += coverage \
    ovidebug
CONFIG(debug,debug|release) { 
    QMAKE_EXTRA_TARGETS += cov_cxxflags \
        cov_lflags \
        ovidebug
    cov_cxxflags.target = coverage
    cov_cxxflags.depends = CXXFLAGS \
        += \
        -fprofile-arcs \
        -ftest-coverage \
        -O0
    cov_lflags.target = coverage
    cov_lflags.depends = LFLAGS \
        += \
        -lgcov \
        -fprofile-arcs \
        -ftest-coverage
    coverage.commands = @echo \
        "Built with coverage support..."
    build_pass|!debug_and_release:coverage.depends = all
    QMAKE_CLEAN += $(OBJECTS_DIR)/*.gcda \
        $(OBJECTS_DIR)/*.gcno
}
TEMPLATE = lib
TARGET = mkcal-qt5
VERSION+= 0.3.13

DEPENDPATH += . \
    klibport \
    versit
INCLUDEPATH += . \
    .. \
    qtlockedfile/src \
    /usr/include/glib-2.0 \
    /usr/lib/glib-2.0/include \
    /usr/include/dbus-1.0

#DEFINES += MEEGO UUID MKCAL_FOR_MEEGO TIMED_SUPPORT

DEFINES += MEEGO UUID TIMED_SUPPORT
PKGCONFIG += timed-qt5

CONFIG += link_pkgconfig
PKGCONFIG += uuid \
    libical \
    sqlite3 \
    libkcalcoren-qt5

QT += dbus
QT -= gui

target.path = $$INSTALL_ROOT/usr/lib
headers.path += $$INSTALL_ROOT/usr/include/mkcal-qt5
headers.files += *.h \
    kdedate/*.h \
    klibport/*.h

pkgconfig.path = $$INSTALL_ROOT/usr/lib/pkgconfig
pkgconfig.files = ../libmkcal-qt5.pc

#CONFIG += qtsparql
INSTALLS += target \
    headers \
    pkgconfig
#QMAKE_CXXFLAGS += -Werror  #in the debian/rules now
QMAKE_CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden
HEADERS += extendedcalendar.h \
    extendedstorage.h \
    notebook.h \
    sqliteformat.h \
    sqlitestorage.h \
    qtlockedfile/src/qtlockedfile.cpp \
    qtlockedfile/src/qtlockedfile_unix.cpp \
    servicehandlerif.h \
    servicehandler.h \
    compatibility.h \
    dummystorage.h \
    mkcal_export.h
SOURCES += extendedcalendar.cpp \
    extendedstorage.cpp \
    notebook.cpp \
    sqliteformat.cpp \
    sqlitestorage.cpp \
    qtlockedfile/src/qtlockedfile.cpp \
    qtlockedfile/src/qtlockedfile_unix.cpp \
    compatibility.cpp \
    servicehandler.cpp
