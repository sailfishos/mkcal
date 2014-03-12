TEMPLATE = app

TARGET = mkcaltool
target.path = /usr/bin

CONFIG += link_pkgconfig

INCLUDEPATH +=  ../../src
QMAKE_LIBDIR += ../../src

LIBS += -lmkcal-qt5
PKGCONFIG += libkcalcoren-qt5


SOURCES += main.cpp \
    mkcaltool.cpp

INSTALLS += target

HEADERS += \
    mkcaltool.h
