
include(../plugins.pri)

unix {
	VPATH       += unix
	INCLUDEPATH += unix
	
	SOURCES += DebuggerCoreUNIX.cpp
	HEADERS += DebuggerCoreUNIX.h

	linux-* {
		VPATH       += unix/linux
		INCLUDEPATH += unix/linux

		# Add detector of broken writes to /proc/pid/mem
		checkProcPidMemWrites.target = $$OUT_PWD/procPidMemWrites.h
		checkProcPidMemWritesOutFile = $$OUT_PWD/proc-pid-mem-write
		
		checkProcPidMemWrites.commands += $$QMAKE_CXX $$QMAKE_CXXFLAGS $$QMAKE_LFLAGS -std=c++11 $$PWD/unix/linux/detect/proc-pid-mem-write.cpp -o $$checkProcPidMemWritesOutFile && \
										  $$OUT_PWD/proc-pid-mem-write $$checkProcPidMemWrites.target
		
		checkProcPidMemWrites.depends += $$PWD/unix/linux/detect/proc-pid-mem-write.cpp
		
		# and its clean target
		procPidMemWriteClean.commands = rm -f $$checkProcPidMemWrites.target $$checkProcPidMemWritesOutFile
		clean.depends = procPidMemWriteClean

		PRE_TARGETDEPS += $$checkProcPidMemWrites.target
		QMAKE_EXTRA_TARGETS += checkProcPidMemWrites procPidMemWriteClean clean
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

HEADERS += PlatformProcess.h   PlatformEvent.h   PlatformState.h   PlatformRegion.h   DebuggerCoreBase.h   DebuggerCore.h   Breakpoint.h   PlatformCommon.h   PlatformThread.h
SOURCES += PlatformProcess.cpp PlatformEvent.cpp PlatformState.cpp PlatformRegion.cpp DebuggerCoreBase.cpp DebuggerCore.cpp Breakpoint.cpp PlatformCommon.cpp PlatformThread.cpp
