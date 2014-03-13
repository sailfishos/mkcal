QT += testlib
CONFIG += link_pkgconfig c++11

INCLUDEPATH +=  ../src
QMAKE_LIBDIR += ../src

LIBS += -lmkcal-qt5
PKGCONFIG += libkcalcoren-qt5 libical

HEADERS += \
    tst_storage.h

SOURCES += \
    tst_storage.cpp
