TEMPLATE = subdirs
SUBDIRS  = src plugins

plugins.subdir  = plugins
plugins.depends = src

# test for usable Qt version
lessThan(QT_VERSION, 4.5) {
	error('edb requires Qt version 4.5 or greater')
}
