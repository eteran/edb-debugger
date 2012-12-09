
EDB_ROOT = ../..

TEMPLATE = lib
CONFIG   += plugin
#CONFIG  += silent
DESTDIR  = $$EDB_ROOT
INSTALLS += target

DEFINES += EDB_PLUGIN

# ignore missing symbols, they'll be found when linked into edb
linux-g++*:QMAKE_LFLAGS -= $$QMAKE_LFLAGS_NOUNDEF
linux-g++*:QMAKE_LFLAGS -= "-Wl,--no-undefined"
macx-g++*:QMAKE_LFLAGS += "-undefined dynamic_lookup"
macx*:QMAKE_LFLAGS += "-undefined dynamic_lookup"

unix {
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
		INCLUDEPATH += $$EDB_ROOT/include/arch/$$QT_ARCH
		DEPENDPATH  += $$EDB_ROOT/include/arch/$$QT_ARCH
		include(plugins-$${QT_ARCH}.pri)
	}

	debug {
		OBJECTS_DIR = $${OUT_PWD}/.debug-shared/obj
		MOC_DIR     = $${OUT_PWD}/.debug-shared/moc
		RCC_DIR     = $${OUT_PWD}/.debug-shared/rcc
		UI_DIR      = $${OUT_PWD}/.debug-shared/uic
	}
	
	release {
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
		INCLUDEPATH += $$EDB_ROOT/include/os/win32 $$EDB_ROOT/include $$EDB_ROOT/include/arch/i386 "C:\\Program Files\\boost\\boost_1_51"
		DEPENDPATH  += $$EDB_ROOT/include/os/win32 $$EDB_ROOT/include $$EDB_ROOT/include/arch/i386
		LIBS        += $$EDB_ROOT/edb.lib
	}
}
