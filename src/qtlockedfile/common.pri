infile(config.pri, SOLUTIONS_LIBRARY, yes): CONFIG += qtlockedfile-uselib
TEMPLATE += fakelib
QTLOCKEDFILE_LIBNAME = $$qtLibraryTarget(QtSolutions_LockedFile-2.4)
TEMPLATE -= fakelib
QTLOCKEDFILE_LIBDIR = $$PWD/lib
unix:qtlockedfile-uselib:!qtlockedfile-buildlib:QMAKE_RPATHDIR += $$QTLOCKEDFILE_LIBDIR
