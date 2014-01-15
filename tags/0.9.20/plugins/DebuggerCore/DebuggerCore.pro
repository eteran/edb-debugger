
include(../plugins.pri)

unix {
	VPATH       += unix
	INCLUDEPATH += unix
	
	SOURCES += DebuggerCoreUNIX.cpp
	HEADERS += DebuggerCoreUNIX.h

	linux-* {
		VPATH       += unix/linux
		INCLUDEPATH += unix/linux
	}

	openbsd-* {
		VPATH       += unix/openbsd
		INCLUDEPATH += unix/openbsd
	}

	freebsd-*{
		VPATH       += unix/freebsd
		INCLUDEPATH += unix/freebsd
	}

	macx {
		VPATH       += unix/osx
		INCLUDEPATH += unix/osx
	}
}

win32 {
	VPATH       += win32 .
	INCLUDEPATH += win32 .
}

HEADERS += PlatformEvent.h   PlatformState.h   PlatformRegion.h   DebuggerCoreBase.h   DebuggerCore.h   X86Breakpoint.h
SOURCES += PlatformEvent.cpp PlatformState.cpp PlatformRegion.cpp DebuggerCoreBase.cpp DebuggerCore.cpp X86Breakpoint.cpp
