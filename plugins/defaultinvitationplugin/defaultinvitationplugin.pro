TEMPLATE = lib
TARGET = defaultinvitationplugin
CONFIG += plugin kcalcoren
OBJECTS_DIR = .obj
MOC_DIR = .moc
INCLUDEPATH +=  . \
                ../.. \
                ../../src \
                /usr/include/qtopiamail \

QMAKE_LIBDIR += ../../src
LIBS += -lkcalcoren
QMAKE_CLEAN += *.so* .obj/* .moc/*
VER_MAJ = ${VER_MAJ}
VER_MIN = ${VER_MIN}
VER_PAT = ${VER_PAT}

DEFINES += MKCAL_FOR_MEEGO

QT -= gui

target.path +=  /${DESTDIR}/usr/lib/calendar/mkcalplugins/

INSTALLS += target

HEADERS +=  defaultinvitationplugin.h \

SOURCES +=  defaultinvitationplugin.cpp \

contains (DEFINES, MKCAL_FOR_MEEGO) {
    LIBS += -lqtopiamail
    HEADERS += transmitemail.h
    SOURCES += transmitemail.cpp
    INCLUDEPATH += /usr/include/qtopiamail
}
