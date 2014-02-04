QT += testlib
CONFIG += link_pkgconfig c++11

INCLUDEPATH +=  ../src
QMAKE_LIBDIR += ../src

equals(QT_MAJOR_VERSION, 4) {
  LIBS += -lmkcal
  PKGCONFIG += libkcalcoren libical
}
equals(QT_MAJOR_VERSION, 5) {
  LIBS += -lmkcal-qt5
  PKGCONFIG += libkcalcoren-qt5 libical
}


HEADERS += \
    tst_storage.h

SOURCES += \
    tst_storage.cpp
