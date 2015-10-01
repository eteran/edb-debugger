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
	INCLUDEPATH += $$LEVEL/include/os/unix
	
	# OS include paths
	openbsd-* : INCLUDEPATH += /usr/local/include
	macx-*    : INCLUDEPATH += /opt/local/include

	# arch include paths
	macx {
		include(plugins-x86_64.pri)
	}

	*-g++-32 {
		include(plugins-x86.pri)	
	} else:*-g++-64 {
		include(plugins-x86_64.pri)
	} else:!macx:contains(QMAKE_HOST.arch, x86_64) {
		include(plugins-x86_64.pri)
	} else:!macx:contains(QMAKE_HOST.arch, i[3456]86) {		
		include(plugins-x86.pri)
	}
	
	QMAKE_CLEAN += $${DESTDIR}/$${QMAKE_PREFIX_SHLIB}$${TARGET}.so
}

win32 {
	win32-msvc*:contains(QMAKE_HOST.arch, x86_64):{
		include(plugins-x86_64.pri)
	}

	win32-msvc*:contains(QMAKE_HOST.arch, i[3456]86):{
		include(plugins-x86.pri)
	}
	
	INCLUDEPATH += $$LEVEL/include/os/win32 
	INCLUDEPATH += "C:\\Program Files\\boost\\boost_1_51"
	LIBS        += $$LEVEL/edb.lib	
}
