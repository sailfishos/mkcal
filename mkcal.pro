# QMake project file for libmkcal

TEMPLATE = subdirs
SUBDIRS = src tests tools

SUBDIRS += plugins
plugins.depends = src

build_doc {
  include(doc/doc.pro)
}

CONFIG += ordered

OTHER_FILES += rpm/*.spec
