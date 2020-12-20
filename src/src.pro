# QMake project file for mkcal sources.
TEMPLATE = lib
TARGET = mkcal-qt5

#DEFINES += MKCAL_FOR_MEEGO TIMED_SUPPORT

DEFINES += MKCALPLUGINDIR=\\\"$$[QT_INSTALL_LIBS]/mkcalplugins\\\"

DEFINES += TIMED_SUPPORT

CONFIG += link_pkgconfig create_pc create_prl no_install_prl c++11
PKGCONFIG += sqlite3 \
    KF5CalendarCore \
    timed-qt5

QT += dbus
QT -= gui

target.path = $$[QT_INSTALL_LIBS]
headers.path = /usr/include/mkcal-qt5
headers.files += *.h \

pkgconfig.path = $$[QT_INSTALL_LIBS]/pkgconfig

INSTALLS += target \
    headers \
    pkgconfig

QMAKE_CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden

HEADERS += extendedcalendar.h \
    extendedstorage.h \
    notebook.h \
    sqliteformat.h \
    sqlitestorage.h \
    servicehandlerif.h \
    servicehandler.h \
    dummystorage.h \
    mkcal_export.h \
    logging_p.h

SOURCES += extendedcalendar.cpp \
    extendedstorage.cpp \
    notebook.cpp \
    sqliteformat.cpp \
    sqlitestorage.cpp \
    servicehandler.cpp \
    logging.cpp

unix {
    HEADERS += \
        semaphore_p.h
    SOURCES += \
        semaphore_p.cpp
}

QMAKE_PKGCONFIG_NAME = $$TARGET
QMAKE_PKGCONFIG_DESCRIPTION = Maemo mkcal calendar library
QMAKE_PKGCONFIG_LIBDIR = $$target.path
QMAKE_PKGCONFIG_INCDIR = $$headers.path
QMAKE_PKGCONFIG_DESTDIR = pkgconfig
