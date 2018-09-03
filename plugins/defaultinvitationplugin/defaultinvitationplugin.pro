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
VER_MAJ = ${VER_MAJ}
VER_MIN = ${VER_MIN}
VER_PAT = ${VER_PAT}

LIBS += -L../../src/ -lmkcal-qt5
CONFIG += link_pkgconfig
PKGCONFIG += qmfclient5 \
    libkcalcoren-qt5

QT -= gui

target.path +=  /${DESTDIR}/usr/lib/mkcalplugins/

INSTALLS += target

HEADERS +=  defaultinvitationplugin.h

SOURCES +=  defaultinvitationplugin.cpp
