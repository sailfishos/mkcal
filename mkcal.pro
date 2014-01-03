# QMake project file for libmkcal

TEMPLATE = subdirs
SUBDIRS = src tests

# add to Qt 5 also when qmfclient is ported
equals(QT_MAJOR_VERSION, 4): SUBDIRS += plugins

VER_MAJ = ${VER_MAJ}
VER_MIN = ${VER_MIN}
VER_PAT = ${VER_PAT}
include(doc/doc.pro)
# make coverage
coverage.CONFIG += recursive
QMAKE_EXTRA_TARGETS += coverage

equals(QT_MAJOR_VERSION, 4) {
    qtconfigfiles.files = mkcal.prf
    qtconfigfiles.path = /usr/share/qt4/mkspecs/features

    INSTALLS += qtconfigfiles
}

CONFIG(debug,debug|release) {

    QMAKE_EXTRA_TARGETS += cov_cxxflags cov_lflags ovi_cxxflags

    cov_cxxflags.target  = coverage
    cov_cxxflags.depends = CXXFLAGS += -fprofile-arcs -ftest-coverage -O0

    cov_lflags.target  = coverage
    cov_lflags.depends = LFLAGS += -lgcov -fprofile-arcs -ftest-coverage

    coverage.commands = @echo "Built with coverage support..."

    QMAKE_CLEAN += $(OBJECTS_DIR)/*.gcda $(OBJECTS_DIR)/*.gcno
}

CONFIG += ordered
