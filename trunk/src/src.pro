TEMPLATE    = app
#CONFIG     += silent
TARGET      = edb
DEPENDPATH  += ./widgets ../include
INCLUDEPATH += ./widgets ../include
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
DEPENDPATH  += ./qhexview
INCLUDEPATH += ./qhexview
SOURCES     += qhexview.cpp
HEADERS     += qhexview.h QHexView

# qjson stuff

DEPENDPATH   += ./qjson
INCLUDEPATH  += ./qjson

HEADERS += json_parser.hh \
           json_scanner.h \
           location.hh \
           parser.h \
           parser_p.h \
           parserrunnable.h \
           position.hh \
           qjson_debug.h \
           qjson_export.h \
           qobjecthelper.h \
           serializer.h \
           serializerrunnable.h \
           stack.hh

SOURCES += json_parser.cc \
           json_scanner.cpp \
           parser.cpp \
           parserrunnable.cpp \
           qobjecthelper.cpp \
           serializer.cpp \
           serializerrunnable.cpp

win32 {
	win32-msvc*:contains(QMAKE_TARGET.arch, x86_64):{
		DEPENDPATH  += ../include/os/win32 arch/x86_64 ../include/arch/x86_64 edisassm
		INCLUDEPATH += ../include/os/win32 arch/x86_64 ../include/arch/x86_64 edisassm "C:\\Program Files\\boost\\boost_1_51"
		DEFINES     += _CRT_SECURE_NO_WARNINGS QJSON_MAKEDLL
		RC_FILE     = edb.rc
	}

	win32-msvc*:contains(QMAKE_TARGET.arch, x86):{
		DEPENDPATH  += ../include/os/win32 arch/i386 ../include/arch/i386 edisassm
		INCLUDEPATH += ../include/os/win32 arch/i386 ../include/arch/i386 edisassm "C:\\Program Files\\boost\\boost_1_51"
		DEFINES     += _CRT_SECURE_NO_WARNINGS QJSON_MAKEDLL
		RC_FILE     = edb.rc
	}
}

unix {
	graph {
		DEPENDPATH  += ./graph
		INCLUDEPATH += ./graph
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
	
	DEPENDPATH  += ../include/os/unix edisassm
	INCLUDEPATH += ../include/os/unix edisassm

	QMAKE_TARGET.arch = $$QMAKE_HOST.arch
	*-g++-32  :QMAKE_TARGET.arch = x86
	*-g++-64  :QMAKE_TARGET.arch = x86_64
	*-clang-32:QMAKE_TARGET.arch = x86
	*-clang-64:QMAKE_TARGET.arch = x86_64

	linux-* {
		DEPENDPATH  += ../include/os/unix/linux
		INCLUDEPATH += ../include/os/unix/linux
	}

	openbsd-* {
		DEPENDPATH  += ../include/os/unix/openbsd
		INCLUDEPATH += ../include/os/unix/openbsd /usr/local/include
	}

	freebsd-* {
		DEPENDPATH  += ../include/os/unix/freebsd
		INCLUDEPATH += ../include/os/unix/freebsd
	}

	macx-* {
		DEPENDPATH  += ../include/os/unix/osx
		INCLUDEPATH += ../include/os/unix/osx /opt/local/include
	}

	macx {
		DEPENDPATH  += arch/x86_64 ../include/arch/x86_64
		INCLUDEPATH += arch/x86_64 ../include/arch/x86_64
	}

	!macx:contains(QMAKE_TARGET.arch, x86_64):{
		DEPENDPATH  += arch/x86_64 ../include/arch/x86_64
		INCLUDEPATH += arch/x86_64 ../include/arch/x86_64
	}
	
	!macx:contains(QMAKE_TARGET.arch, x86):{
		DEPENDPATH  += arch/i386 ../include/arch/i386
		INCLUDEPATH += arch/i386 ../include/arch/i386
	}

	*-g++* {
		QMAKE_CXXFLAGS       += -ansi -pedantic -W -Wall -Wno-long-long -Wnon-virtual-dtor
		QMAKE_CXXFLAGS_DEBUG += -g3
	}

	linux-g++*:     QMAKE_CXXFLAGS += -Wstrict-null-sentinel

	# linker flags
	freebsd-g++*: QMAKE_LFLAGS += -lkvm -Wl,--export-dynamic $$(LDFLAGS)
	linux-clang*: QMAKE_LFLAGS += -rdynamic $$(LDFLAGS)
	linux-g++*:   QMAKE_LFLAGS += -rdynamic $$(LDFLAGS)
	macx-clang*:  QMAKE_LFLAGS += -rdynamic $$(LDFLAGS)
	macx-g++*:    QMAKE_LFLAGS += -rdynamic $$(LDFLAGS)
	openbsd-g++*: QMAKE_LFLAGS += -lkvm -Wl,--export-dynamic $$(LDFLAGS)

	CONFIG(debug, debug|release) {
		OBJECTS_DIR = $${OUT_PWD}/.debug-shared/obj
		MOC_DIR     = $${OUT_PWD}/.debug-shared/moc
		RCC_DIR     = $${OUT_PWD}/.debug-shared/rcc
		UI_DIR      = $${OUT_PWD}/.debug-shared/uic
		DEFINES    += QT_SHAREDPOINTER_TRACK_POINTERS
	}

	CONFIG(release, debug|release) {
		OBJECTS_DIR = $${OUT_PWD}/.release-shared/obj
		MOC_DIR     = $${OUT_PWD}/.release-shared/moc
		RCC_DIR     = $${OUT_PWD}/.release-shared/rcc
		UI_DIR      = $${OUT_PWD}/.release-shared/uic
	}
}
