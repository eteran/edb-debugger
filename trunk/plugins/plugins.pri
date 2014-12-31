LEVEL = ../..

include(../qmake/clean-objects.pri)
include(../qmake/c++11.pri)
include(../qmake/qt5-gui.pri)

TEMPLATE = lib
CONFIG   += plugin
DESTDIR  = $$LEVEL
INSTALLS += target

# ignore missing symbols, they'll be found when linked into edb
linux-g++*:      QMAKE_LFLAGS -= $$QMAKE_LFLAGS_NOUNDEF
linux-g++*:      QMAKE_LFLAGS -= "-Wl,--no-undefined"
linux-clang++*:  QMAKE_LFLAGS -= $$QMAKE_LFLAGS_NOUNDEF
linux-clang++*:  QMAKE_LFLAGS -= "-Wl,--no-undefined"
macx-g++*:       QMAKE_LFLAGS += "-undefined dynamic_lookup"
macx-clang*:     QMAKE_LFLAGS += "-undefined dynamic_lookup"

linux-g++*:QMAKE_LFLAGS += $$(LDFLAGS)

unix {
	INCLUDEPATH += $$LEVEL/include $$LEVEL/include/os/unix
	
	# OS include paths
	openbsd-* : INCLUDEPATH += /usr/local/include
	macx-*    : INCLUDEPATH += /opt/local/include

	# arch include paths
	macx {
		INCLUDEPATH += $$LEVEL/include/arch/x86_64
		include(plugins-x86_64.pri)
	}

	!macx:contains(QMAKE_HOST.arch, x86_64) {
		INCLUDEPATH += $$LEVEL/include/arch/x86_64
		include(plugins-x86_64.pri)
	}
	
	!macx:contains(QMAKE_HOST.arch, i[3456]86) {
		INCLUDEPATH += $$LEVEL/include/arch/x86
		include(plugins-x86.pri)
	}
}

win32 {
	win32-msvc*:contains(QMAKE_HOST.arch, x86_64):{
		INCLUDEPATH += $$LEVEL/include/os/win32 $$LEVEL/include $$LEVEL/include/arch/x86_64 "C:\\Program Files\\boost\\boost_1_51"
		LIBS        += $$LEVEL/edb.lib
	}

	win32-msvc*:contains(QMAKE_HOST.arch, i[3456]86):{
		INCLUDEPATH += $$LEVEL/include/os/win32 $$LEVEL/include $$LEVEL/include/arch/x86 "C:\\Program Files\\boost\\boost_1_51"
		LIBS        += $$LEVEL/edb.lib
	}
}
