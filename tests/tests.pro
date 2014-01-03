QT += testlib
CONFIG += link_pkgconfig c++11

PKGCONFIG += libkcalcoren-qt5 libmkcal-qt5 libical

HEADERS += \
    tst_storage.h

SOURCES += \
    tst_storage.cpp
