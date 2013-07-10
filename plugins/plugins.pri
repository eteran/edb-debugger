LEVEL = ../..

include($$LEVEL/qmake/clean-objects.pri)
include($$LEVEL/qmake/target-arch.pri)

TEMPLATE = lib
CONFIG   += plugin
#CONFIG  += silent
DESTDIR  = $$LEVEL
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
	VPATH       += $$LEVEL/include $$LEVEL/include/os/unix
	INCLUDEPATH += $$LEVEL/include $$LEVEL/include/os/unix
	
	# OS include paths
	linux-* {
		VPATH       += $$LEVEL/include/os/unix/linux
		INCLUDEPATH += $$LEVEL/include/os/unix/linux
	}

	openbsd-* {
		VPATH       += $$LEVEL/include/os/unix/openbsd
		INCLUDEPATH += $$LEVEL/include/os/unix/openbsd /usr/local/include
	}

	freebsd-* {
		VPATH       += $$LEVEL/include/os/unix/freebsd
		INCLUDEPATH += $$LEVEL/include/os/unix/freebsd
	}

	macx-* {
		VPATH       += $$LEVEL/include/os/unix/osx
		INCLUDEPATH += $$LEVEL/include/os/unix/osx /opt/local/include
	}


	# arch include paths
	macx {
		INCLUDEPATH += $$LEVEL/include/arch/x86_64
		VPATH       += $$LEVEL/include/arch/x86_64
		include(plugins-x86_64.pri)
	}
	
	!macx {
		INCLUDEPATH += $$LEVEL/include/arch/$${QMAKE_TARGET.arch}
		VPATH       += $$LEVEL/include/arch/$${QMAKE_TARGET.arch}
		include(plugins-$${QMAKE_TARGET.arch}.pri)
	}
}

win32 {
	win32-msvc*:contains(QMAKE_HOST.arch, x86_64):{
		INCLUDEPATH += $$LEVEL/include/os/win32 $$LEVEL/include $$LEVEL/include/arch/x86_64 "C:\\Program Files\\boost\\boost_1_51"
		VPATH       += $$LEVEL/include/os/win32 $$LEVEL/include $$LEVEL/include/arch/x86_64
		LIBS        += $$LEVEL/edb.lib
	}

	win32-msvc*:contains(QMAKE_HOST.arch, x86):{
		INCLUDEPATH += $$LEVEL/include/os/win32 $$LEVEL/include $$LEVEL/include/arch/x86 "C:\\Program Files\\boost\\boost_1_51"
		VPATH       += $$LEVEL/include/os/win32 $$LEVEL/include $$LEVEL/include/arch/x86
		LIBS        += $$LEVEL/edb.lib
	}
}
