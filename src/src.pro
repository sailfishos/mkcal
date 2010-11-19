# QMake project file for mkcal sources.
# make coverage
coverage.CONFIG += recursive
QMAKE_EXTRA_TARGETS += coverage \
    ovidebug
CONFIG(debug,debug|release) { 
    QMAKE_EXTRA_TARGETS += cov_cxxflags \
        cov_lflags \
        ovidebug
    cov_cxxflags.target = coverage
    cov_cxxflags.depends = CXXFLAGS \
        += \
        -fprofile-arcs \
        -ftest-coverage \
        -O0
    cov_lflags.target = coverage
    cov_lflags.depends = LFLAGS \
        += \
        -lgcov \
        -fprofile-arcs \
        -ftest-coverage
    coverage.commands = @echo \
        "Built with coverage support..."
    build_pass|!debug_and_release:coverage.depends = all
    QMAKE_CLEAN += $(OBJECTS_DIR)/*.gcda \
        $(OBJECTS_DIR)/*.gcno
}
TEMPLATE = lib
TARGET = mkcal
VER_MAJ = ${VER_MAJ}
VER_MIN = ${VER_MIN}
VER_PAT = ${VER_PAT}

DEPENDPATH += . \
    klibport \
    versit
INCLUDEPATH += . \
    .. \
    qtlockedfile/src \
    /usr/include/libical \
    /usr/include/glib-2.0 \
    /usr/lib/glib-2.0/include \
    /usr/include/dbus-1.0 \
    /usr/include/qt4/QtDBus \
    /usr/include/accounts-qt


DEFINES += MEEGO UUID MKCAL_FOR_MEEGO TIMED_SUPPORT
LIBS += -lQtDBus \
    -lsqlite3 \
    -luuid \

contains (DEFINES, TIMED_SUPPORT) {
     LIBS += -ltimed
}

contains (DEFINES, MKCAL_FOR_MEEGO) {
    LIBS += -lmeegotouchcore -laccounts-qt
    INCLUDEPATH += /usr/include/meegotouch
}

QT -= gui
QT += xml

QMAKE_CLEAN += lib*.so*
libraries.path += /${DESTDIR}/usr/lib
libraries.files += lib*.so.*.*.*
headers.path += /${DESTDIR}/usr/include/mkcal
headers.files += *.h \
    kdedate/*.h \
    klibport/*.h
pkgconfig.path += /${DESTDIR}/usr/lib/pkgconfig
pkgconfig.files += ../*.pc
CONFIG += kcalcoren
INSTALLS += libraries \
    headers \
    pkgconfig
#QMAKE_CXXFLAGS += -Werror  #in the debian/rules now
QMAKE_CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden
HEADERS += extendedcalendar.h \
    directorystorage.h \
    extendedstorage.h \
    notebook.h \
    sqliteformat.h \
    sqlitestorage.h \
    trackerformat.h \
    trackermodify.h \
    trackerstorage.h \
    qtlockedfile/src/qtlockedfile.cpp \
    qtlockedfile/src/qtlockedfile_unix.cpp \
    servicehandlerif.h \
    servicehandler.h \
    compatibility.h \
    dummystorage.h \
    mkcal_export.h
SOURCES += extendedcalendar.cpp \
    directorystorage.cpp \
    extendedstorage.cpp \
    notebook.cpp \
    sqliteformat.cpp \
    sqlitestorage.cpp \
    trackerformat.cpp \
    trackermodify.cpp \
    trackerstorage.cpp \
    qtlockedfile/src/qtlockedfile.cpp \
    qtlockedfile/src/qtlockedfile_unix.cpp \
    compatibility.cpp \
    servicehandler.cpp
