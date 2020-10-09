TEMPLATE = lib
TARGET = $$qtLibraryTarget(defaultinvitationplugin)
CONFIG += plugin
OBJECTS_DIR = .obj
MOC_DIR = .moc
INCLUDEPATH +=  . \
                ../.. \
                ../../src \

QMAKE_LIBDIR += ../../src
QMAKE_CLEAN += *.so* .obj/* .moc/*

LIBS += -L../../src/ -lmkcal-qt5
CONFIG += link_pkgconfig
PKGCONFIG += QmfClient \
    KF5CalendarCore

QT -= gui

target.path +=  /${DESTDIR}$$[QT_INSTALL_LIBS]/mkcalplugins/

INSTALLS += target

HEADERS +=  defaultinvitationplugin.h

SOURCES +=  defaultinvitationplugin.cpp
