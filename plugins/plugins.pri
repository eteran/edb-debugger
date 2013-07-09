
EDB_ROOT = ../..

TEMPLATE = lib
CONFIG   += plugin
#CONFIG  += silent
DESTDIR  = $$EDB_ROOT
INSTALLS += target
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

DEFINES += EDB_PLUGIN

# ignore missing symbols, they'll be found when linked into edb
linux-g++*: QMAKE_LFLAGS -= $$QMAKE_LFLAGS_NOUNDEF
linux-g++*: QMAKE_LFLAGS -= "-Wl,--no-undefined"
macx-g++*:  QMAKE_LFLAGS += "-undefined dynamic_lookup"
macx*:      QMAKE_LFLAGS += "-undefined dynamic_lookup"

linux-g++*:QMAKE_LFLAGS += $$(LDFLAGS)

unix {

	QMAKE_TARGET.arch = $$QMAKE_HOST.arch
	*-g++-32  :QMAKE_TARGET.arch = x86
	*-g++-64  :QMAKE_TARGET.arch = x86_64
	*-clang-32:QMAKE_TARGET.arch = x86
	*-clang-64:QMAKE_TARGET.arch = x86_64

	# generic unix include paths
	DEPENDPATH  += $$EDB_ROOT/include $$EDB_ROOT/include/os/unix
	INCLUDEPATH += $$EDB_ROOT/include $$EDB_ROOT/include/os/unix
	
	# OS include paths
	linux-* {
		DEPENDPATH  += $$EDB_ROOT/include/os/unix/linux
		INCLUDEPATH += $$EDB_ROOT/include/os/unix/linux
	}

	openbsd-* {
		DEPENDPATH  += $$EDB_ROOT/include/os/unix/openbsd
		INCLUDEPATH += $$EDB_ROOT/include/os/unix/openbsd /usr/local/include
	}

	freebsd-* {
		DEPENDPATH  += $$EDB_ROOT/include/os/unix/freebsd
		INCLUDEPATH += $$EDB_ROOT/include/os/unix/freebsd
	}

	macx-* {
		DEPENDPATH  += $$EDB_ROOT/include/os/unix/osx
		INCLUDEPATH += $$EDB_ROOT/include/os/unix/osx /opt/local/include
	}


	# arch include paths
	macx {
		INCLUDEPATH += $$EDB_ROOT/include/arch/x86_64
		DEPENDPATH  += $$EDB_ROOT/include/arch/x86_64
		include(plugins-x86_64.pri)
	}
	
	!macx {
		INCLUDEPATH += $$EDB_ROOT/include/arch/$${QMAKE_TARGET.arch}
		DEPENDPATH  += $$EDB_ROOT/include/arch/$${QMAKE_TARGET.arch}
		include(plugins-$${QMAKE_TARGET.arch}.pri)
	}

	CONFIG(debug, debug|release) {
		DEFINES    += QT_SHAREDPOINTER_TRACK_POINTERS
		OBJECTS_DIR = $${OUT_PWD}/.debug-shared/obj
		MOC_DIR     = $${OUT_PWD}/.debug-shared/moc
		RCC_DIR     = $${OUT_PWD}/.debug-shared/rcc
		UI_DIR      = $${OUT_PWD}/.debug-shared/uic
	}
	
	CONFIG(release, debug|release) {
		OBJECTS_DIR = $${OUT_PWD}/.release-shared/obj
		MOC_DIR     = $${OUT_PWD}/.release-shared/moc
		RCC_DIR     = $${OUT_PWD}/.release-shared/rcc
		UI_DIR      = $${OUT_PWD}/.release-shared/uic
	}
}

win32 {
	win32-msvc*:contains(QMAKE_HOST.arch, x86_64):{
		INCLUDEPATH += $$EDB_ROOT/include/os/win32 $$EDB_ROOT/include $$EDB_ROOT/include/arch/x86_64 "C:\\Program Files\\boost\\boost_1_51"
		DEPENDPATH  += $$EDB_ROOT/include/os/win32 $$EDB_ROOT/include $$EDB_ROOT/include/arch/x86_64
		LIBS        += $$EDB_ROOT/edb.lib
	}

	win32-msvc*:contains(QMAKE_HOST.arch, x86):{
		INCLUDEPATH += $$EDB_ROOT/include/os/win32 $$EDB_ROOT/include $$EDB_ROOT/include/arch/x86 "C:\\Program Files\\boost\\boost_1_51"
		DEPENDPATH  += $$EDB_ROOT/include/os/win32 $$EDB_ROOT/include $$EDB_ROOT/include/arch/x86
		LIBS        += $$EDB_ROOT/edb.lib
	}
}
