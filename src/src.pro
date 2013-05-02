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
equals(QT_MAJOR_VERSION, 4): TARGET = mkcal
equals(QT_MAJOR_VERSION, 5): TARGET = mkcal-qt5
VERSION+= 0.3.11

DEPENDPATH += . \
    klibport \
    versit
INCLUDEPATH += . \
    .. \
    qtlockedfile/src \
    /usr/include/glib-2.0 \
    /usr/lib/glib-2.0/include \
    /usr/include/dbus-1.0

#DEFINES += MEEGO UUID MKCAL_FOR_MEEGO TIMED_SUPPORT MKCAL_TRACKER_SYNC
DEFINES += MEEGO UUID TIMED_SUPPORT #MKCAL_TRACKER_SYNC

CONFIG += link_pkgconfig
PKGCONFIG += uuid \
    libical \
    sqlite3

equals(QT_MAJOR_VERSION, 4): PKGCONFIG += timed libkcalcoren
equals(QT_MAJOR_VERSION, 5): PKGCONFIG += timed-qt5 libkcalcoren-qt5

QT += dbus

contains (DEFINES, MKCAL_FOR_MEEGO) {
    LIBS += -lmeegotouchcore
    INCLUDEPATH += /usr/include/meegotouch
}

QT -= gui

target.path = $$INSTALL_ROOT/usr/lib
equals(QT_MAJOR_VERSION, 4): headers.path += $$INSTALL_ROOT/usr/include/mkcal
equals(QT_MAJOR_VERSION, 5): headers.path += $$INSTALL_ROOT/usr/include/mkcal-qt5
headers.files += *.h \
    kdedate/*.h \
    klibport/*.h

pkgconfig.path = $$INSTALL_ROOT/usr/lib/pkgconfig
equals(QT_MAJOR_VERSION, 4): pkgconfig.files = ../libmkcal.pc
equals(QT_MAJOR_VERSION, 5): pkgconfig.files = ../libmkcal-qt5.pc

#CONFIG += qtsparql
INSTALLS += target \
    headers \
    pkgconfig
#QMAKE_CXXFLAGS += -Werror  #in the debian/rules now
QMAKE_CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden
HEADERS += extendedcalendar.h \
    directorystorage.h \
    extendedstorage.h \
    notebook.h \
    sqliteformat.h \
    sqlitestorage.h \
    trackerformat.h \
    trackermodify.h \
    trackerstorage.h \
    qtlockedfile/src/qtlockedfile.cpp \
    qtlockedfile/src/qtlockedfile_unix.cpp \
    servicehandlerif.h \
    servicehandler.h \
    compatibility.h \
    dummystorage.h \
    mkcal_export.h
SOURCES += extendedcalendar.cpp \
    directorystorage.cpp \
    extendedstorage.cpp \
    notebook.cpp \
    sqliteformat.cpp \
    sqlitestorage.cpp \
    trackerformat.cpp \
    trackermodify.cpp \
    trackerstorage.cpp \
    qtlockedfile/src/qtlockedfile.cpp \
    qtlockedfile/src/qtlockedfile_unix.cpp \
    compatibility.cpp \
    servicehandler.cpp
