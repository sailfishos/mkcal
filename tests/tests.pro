TEMPLATE = app
TARGET = tst_storage

QT += testlib dbus
CONFIG += link_pkgconfig c++11

DEFINES += TIMED_SUPPORT

INCLUDEPATH +=  ../src
QMAKE_LIBDIR += ../src

LIBS += -lmkcal-qt5
PKGCONFIG += libkcalcoren-qt5 sqlite3 timed-qt5

HEADERS += \
    tst_storage.h

SOURCES += \
    tst_storage.cpp

target.path = /opt/tests/mkcal/

INSTALLS += target
