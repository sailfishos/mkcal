TEMPLATE = app

TARGET = mkcaltool
target.path = /usr/bin

CONFIG += link_pkgconfig

PKGCONFIG += libkcalcoren-qt5 libmkcal-qt5

SOURCES += main.cpp \
    mkcaltool.cpp

INSTALLS += target

HEADERS += \
    mkcaltool.h
