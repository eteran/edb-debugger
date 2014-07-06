LEVEL = ../..

include(../qmake/clean-objects.pri)

TEMPLATE = lib
CONFIG   += plugin
DESTDIR  = $$LEVEL
INSTALLS += target
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
	CONFIG += c++11
}

linux-g++ {
    system(g++ -dumpversion | grep -e "^4\\.[7-9]\\.[0-9]$" > /dev/null) {
		QMAKE_CXXFLAGS += -std=c++11
	}
}


# ignore missing symbols, they'll be found when linked into edb
linux-g++*: QMAKE_LFLAGS -= $$QMAKE_LFLAGS_NOUNDEF
linux-g++*: QMAKE_LFLAGS -= "-Wl,--no-undefined"
macx-g++*:  QMAKE_LFLAGS += "-undefined dynamic_lookup"
macx*:      QMAKE_LFLAGS += "-undefined dynamic_lookup"

linux-g++*:QMAKE_LFLAGS += $$(LDFLAGS)

unix {
	INCLUDEPATH += $$LEVEL/include $$LEVEL/include/os/unix
	
	# OS include paths
	linux-*   : INCLUDEPATH += $$LEVEL/include/os/unix/linux
	openbsd-* : INCLUDEPATH += $$LEVEL/include/os/unix/openbsd /usr/local/include
	freebsd-* : INCLUDEPATH += $$LEVEL/include/os/unix/freebsd
	macx-*    : INCLUDEPATH += $$LEVEL/include/os/unix/osx /opt/local/include

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
