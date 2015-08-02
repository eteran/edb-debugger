include(common.pri)

TEMPLATE = subdirs
SUBDIRS  = src plugins

plugins.subdir  = plugins
plugins.depends = src

# test for usable Qt version
lessThan(QT_VERSION, 4.6) {
	error('edb requires Qt version 4.6 or greater')
}
