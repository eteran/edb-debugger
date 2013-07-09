TEMPLATE    = app
TARGET      = edb
DEPENDPATH  += $$PWD/widgets $$PWD/../include
INCLUDEPATH += $$PWD/widgets $$PWD/../include
VPATH       += $$PWD/widgets $$PWD/../include

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
DEPENDPATH  += $$PWD/qhexview
INCLUDEPATH += $$PWD/qhexview
VPATH       += $$PWD/qhexview

SOURCES     += qhexview.cpp
HEADERS     += qhexview.h QHexView

win32 {
	win32-msvc*:contains(QMAKE_TARGET.arch, x86_64):{
		VPATH       += $$PWD/../include/os/win32 $$PWD/arch/x86_64 $$PWD/../include/arch/x86_64 $$PWD/edisassm
		DEPENDPATH  += $$PWD/../include/os/win32 $$PWD/arch/x86_64 $$PWD/../include/arch/x86_64 $$PWD/edisassm
		INCLUDEPATH += $$PWD/../include/os/win32 $$PWD/arch/x86_64 $$PWD/../include/arch/x86_64 $$PWD/edisassm "C:\\Program Files\\boost\\boost_1_51"
		DEFINES     += _CRT_SECURE_NO_WARNINGS QJSON_MAKEDLL
		RC_FILE     = edb.rc
	}

	win32-msvc*:contains(QMAKE_TARGET.arch, x86):{
		VPATH       += $$PWD/../include/os/win32 $$PWD/arch/x86 $$PWD/../include/arch/x86 $$PWD/edisassm
		DEPENDPATH  += $$PWD/../include/os/win32 $$PWD/arch/x86 $$PWD/../include/arch/x86 $$PWD/edisassm
		INCLUDEPATH += $$PWD/../include/os/win32 $$PWD/arch/x86 $$PWD/../include/arch/x86 $$PWD/edisassm "C:\\Program Files\\boost\\boost_1_51"
		DEFINES     += _CRT_SECURE_NO_WARNINGS QJSON_MAKEDLL
		RC_FILE     = edb.rc
	}
}

unix {
	graph {
		VPATH       += $$PWD/graph
		DEPENDPATH  += $$PWD/graph
		INCLUDEPATH += $$PWD/graph
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
	
	VPATH       += $$PWD/../include/os/unix $$PWD/edisassm
	DEPENDPATH  += $$PWD/../include/os/unix $$PWD/edisassm
	INCLUDEPATH += $$PWD/../include/os/unix $$PWD/edisassm

	QMAKE_TARGET.arch = $$QMAKE_HOST.arch
	*-g++-32  :QMAKE_TARGET.arch = x86
	*-g++-64  :QMAKE_TARGET.arch = x86_64
	*-clang-32:QMAKE_TARGET.arch = x86
	*-clang-64:QMAKE_TARGET.arch = x86_64

	linux-* {
		VPATH       += $$PWD/../include/os/unix/linux
		DEPENDPATH  += $$PWD/../include/os/unix/linux
		INCLUDEPATH += $$PWD/../include/os/unix/linux
	}

	openbsd-* {
		VPATH       += $$PWD/../include/os/unix/openbsd
		DEPENDPATH  += $$PWD/../include/os/unix/openbsd
		INCLUDEPATH += $$PWD/../include/os/unix/openbsd /usr/local/include
	}

	freebsd-* {
		VPATH       += $$PWD/../include/os/unix/freebsd
		DEPENDPATH  += $$PWD/../include/os/unix/freebsd
		INCLUDEPATH += $$PWD/../include/os/unix/freebsd
	}

	macx-* {
		VPATH       += $$PWD/../include/os/unix/osx
		DEPENDPATH  += $$PWD/../include/os/unix/osx
		INCLUDEPATH += $$PWD/../include/os/unix/osx /opt/local/include
	}

	macx {
		VPATH       += $$PWD/arch/x86_64 $$PWD/../include/arch/x86_64
		DEPENDPATH  += $$PWD/arch/x86_64 $$PWD/../include/arch/x86_64
		INCLUDEPATH += $$PWD/arch/x86_64 $$PWD/../include/arch/x86_64
	}

	!macx:contains(QMAKE_TARGET.arch, x86_64):{
		VPATH       += $$PWD/arch/x86_64 $$PWD/../include/arch/x86_64
		DEPENDPATH  += $$PWD/arch/x86_64 $$PWD/../include/arch/x86_64
		INCLUDEPATH += $$PWD/arch/x86_64 $$PWD/../include/arch/x86_64
	}
	
	!macx:contains(QMAKE_TARGET.arch, x86):{
		VPATH       += $$PWD/arch/x86 $$PWD/../include/arch/x86
		DEPENDPATH  += $$PWD/arch/x86 $$PWD/../include/arch/x86
		INCLUDEPATH += $$PWD/arch/x86 $$PWD/../include/arch/x86
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
