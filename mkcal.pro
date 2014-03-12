# QMake project file for libmkcal

TEMPLATE = subdirs
SUBDIRS = src tests tools

# add to Qt 5 also when qmfclient is ported
#SUBDIRS += plugins

include(doc/doc.pro)
# make coverage
coverage.CONFIG += recursive
QMAKE_EXTRA_TARGETS += coverage

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

OTHER_FILES += rpm/*.spec
