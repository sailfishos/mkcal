TEMPLATE = app
TARGET = tst_storage

QT += testlib
CONFIG += link_pkgconfig c++11

INCLUDEPATH +=  ../src
QMAKE_LIBDIR += ../src

LIBS += -lmkcal-qt5
PKGCONFIG += libkcalcoren-qt5

HEADERS += \
    tst_storage.h

SOURCES += \
    tst_storage.cpp

target.path = /opt/tests/mkcal/

INSTALLS += target
