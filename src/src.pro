# QMake project file for mkcal sources.
TEMPLATE = lib
TARGET = mkcal-qt5
VERSION+= 0.4.0

DEPENDPATH += . \
    klibport \
    versit
INCLUDEPATH += . \
    .. \

#DEFINES += MEEGO UUID MKCAL_FOR_MEEGO TIMED_SUPPORT

DEFINES += MEEGO UUID TIMED_SUPPORT

CONFIG += link_pkgconfig
PKGCONFIG += uuid \
    libical \
    sqlite3 \
    libkcalcoren-qt5 \
    timed-qt5

QT += dbus
QT -= gui

target.path = /usr/lib
headers.path = /usr/include/mkcal-qt5
headers.files += *.h \

pkgconfig.path = /usr/lib/pkgconfig
pkgconfig.files = ../libmkcal-qt5.pc

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
    mkcal_export.h

SOURCES += extendedcalendar.cpp \
    extendedstorage.cpp \
    notebook.cpp \
    sqliteformat.cpp \
    sqlitestorage.cpp \
    servicehandler.cpp

unix {
    HEADERS += \
        semaphore_p.h
    SOURCES += \
        semaphore_p.cpp
}
