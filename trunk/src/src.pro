LEVEL = ..

include(../qmake/clean-objects.pri)

TEMPLATE    = app
TARGET      = edb
INCLUDEPATH += widgets $$LEVEL/include
VPATH       += widgets $$LEVEL/include

RESOURCES   = debugger.qrc
DESTDIR     = ../
target.path = /bin/
INSTALLS    += target
QT          += xml xmlpatterns

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

TRANSLATIONS += \
	lang/edb_en.ts

HEADERS += \
	API.h \
	ArchProcessor.h \
	ArchTypes.h \
	BinaryString.h \
	ByteShiftArray.h \
	CommentServer.h \
	Configuration.h \
	DataViewInfo.h \
	Debugger.h \
	DebuggerInternal.h \
	DialogArguments.h \
	DialogAttach.h \
	DialogInputBinaryString.h \
	DialogInputValue.h \
	DialogMemoryRegions.h \
	DialogOptions.h \
	DialogPlugins.h \
	DialogThreads.h \
	Expression.h \
	Function.h \
	HexStringValidator.h \
	IAnalyzer.h \
	IArchProcessor.h \
	IBinary.h \
	IBreakpoint.h \
	IDebugEvent.h \
	IDebugEventHandler.h \
	IDebuggerCore.h \
	IPlugin.h \
	IRegion.h \
	ISessionFile.h \
	IState.h \
	ISymbolManager.h \
	Instruction.h \
	LineEdit.h \
	MD5.h \
	MemoryRegions.h \
	Module.h \
	OSTypes.h \
	Process.h \
	RegisterListWidget.h \
	QDisassemblyView.h \
	QLongValidator.h \
	QULongValidator.h \
	RecentFileManager.h \
	RegionBuffer.h \
	Register.h \
	RegisterViewDelegate.h \
	ScopedPointer.h \
	ShiftBuffer.h \
	State.h \
	Symbol.h \
	SymbolManager.h \
	SyntaxHighlighter.h \
	TabWidget.h \
	Types.h \
	Util.h \
	edb.h \
	string_hash.h \
	symbols.h \
	version.h

FORMS += \
	BinaryString.ui \
	Debugger.ui \
	DialogArguments.ui \
	DialogAttach.ui \
	DialogInputBinaryString.ui \
	DialogInputValue.ui \
	DialogMemoryRegions.ui \
	DialogOptions.ui \
	DialogPlugins.ui \
	DialogThreads.ui

SOURCES += \
	ArchProcessor.cpp \
	BinaryString.cpp \
	ByteShiftArray.cpp \
	CommentServer.cpp \
	Configuration.cpp \
	DataViewInfo.cpp \
	Debugger.cpp \
	DialogArguments.cpp \
	DialogAttach.cpp \
	DialogInputBinaryString.cpp \
	DialogInputValue.cpp \
	DialogMemoryRegions.cpp \
	DialogOptions.cpp \
	DialogPlugins.cpp \
	DialogThreads.cpp \
	HexStringValidator.cpp \
	IBinary.cpp \
	LineEdit.cpp \
	MD5.cpp \
	MemoryRegions.cpp \
	RegisterListWidget.cpp \
	QDisassemblyView.cpp \
	QLongValidator.cpp \
	QULongValidator.cpp \
	RecentFileManager.cpp \
	RegionBuffer.cpp \
	Register.cpp \
	RegisterViewDelegate.cpp \
	State.cpp \
	SymbolManager.cpp \
	SyntaxHighlighter.cpp \
	TabWidget.cpp \
	edb.cpp \
	instruction.cpp \
	main.cpp \
	symbols.cpp

# QHexView stuff
INCLUDEPATH += qhexview
VPATH       += qhexview
SOURCES     += qhexview.cpp
HEADERS     += qhexview.h QHexView

win32 {
	win32-msvc*:contains(QMAKE_HOST.arch, x86_64) {
		VPATH       += $$LEVEL/include/os/win32 arch/x86_64 $$LEVEL/include/arch/x86_64 edisassm
		INCLUDEPATH += $$LEVEL/include/os/win32 arch/x86_64 $$LEVEL/include/arch/x86_64 edisassm "C:\\Program Files\\boost\\boost_1_51"
		DEFINES     += _CRT_SECURE_NO_WARNINGS QJSON_MAKEDLL
		RC_FILE     = edb.rc
	}

	win32-msvc*:contains(QMAKE_HOST.arch, i[3456]86) {
		VPATH       += $$LEVEL/include/os/win32 arch/x86 $$LEVEL/include/arch/x86 edisassm
		INCLUDEPATH += $$LEVEL/include/os/win32 arch/x86 $$LEVEL/include/arch/x86 edisassm "C:\\Program Files\\boost\\boost_1_51"
		DEFINES     += _CRT_SECURE_NO_WARNINGS QJSON_MAKEDLL
		RC_FILE     = edb.rc
	}
}

unix {
	graph {
		VPATH       += graph
		INCLUDEPATH += graph
		HEADERS     += GraphEdge.h   GraphWidget.h   GraphNode.h
		SOURCES     += GraphEdge.cpp GraphWidget.cpp GraphNode.cpp
		LIBS        += -lgvc
	}

	!isEmpty(DEFAULT_PLUGIN_PATH) {
		DEFINES += DEFAULT_PLUGIN_PATH=$$DEFAULT_PLUGIN_PATH
	}

	!isEmpty(DEFAULT_SYMBOL_PATH) {
		DEFINES += DEFAULT_SYMBOL_PATH=$$DEFAULT_SYMBOL_PATH
	}

	!isEmpty(DEFAULT_SESSION_PATH) {
		DEFINES += DEFAULT_SESSION_PATH=DEFAULT_SESSION_PATH
	}
	
	VPATH       += $$LEVEL/include/os/unix edisassm
	INCLUDEPATH += $$LEVEL/include/os/unix edisassm

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

	macx {
		VPATH       += arch/x86_64 $$LEVEL/include/arch/x86_64
		INCLUDEPATH += arch/x86_64 $$LEVEL/include/arch/x86_64
	}

	!macx:contains(QMAKE_HOST.arch, x86_64) {
		VPATH       += arch/x86_64 $$LEVEL/include/arch/x86_64
		INCLUDEPATH += arch/x86_64 $$LEVEL/include/arch/x86_64
	}
	
	!macx:contains(QMAKE_HOST.arch, i[3456]86) {
		VPATH       += arch/x86 $$LEVEL/include/arch/x86
		INCLUDEPATH += arch/x86 $$LEVEL/include/arch/x86
	}

	*-g++* | *-clang* {
		QMAKE_CXXFLAGS_DEBUG += -g3
	}

	# linker flags
	freebsd-g++* : QMAKE_LFLAGS += -lkvm -Wl,--export-dynamic $$(LDFLAGS)
	linux-clang* : QMAKE_LFLAGS += -rdynamic $$(LDFLAGS)
	linux-g++*   : QMAKE_LFLAGS += -rdynamic $$(LDFLAGS)
	macx-clang*  : QMAKE_LFLAGS += -rdynamic $$(LDFLAGS)
	macx-g++*    : QMAKE_LFLAGS += -rdynamic $$(LDFLAGS)
	openbsd-g++* : QMAKE_LFLAGS += -lkvm -Wl,--export-dynamic $$(LDFLAGS)
}

