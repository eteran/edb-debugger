include(../common.pri)
LEVEL = ..

include(../qmake/clean-objects.pri)
include(../qmake/c++11.pri)
include(../qmake/qt5-gui.pri)

TEMPLATE    = app
TARGET      = edb
INCLUDEPATH += widgets $$LEVEL/include $$LEVEL/src/capstone-edb/include
VPATH       += widgets $$LEVEL/include

RESOURCES   = debugger.qrc
DESTDIR     = ../
target.path = $$PREFIX/bin/
INSTALLS    += target
QT          += xml xmlpatterns

TRANSLATIONS += \
	lang/edb_en.ts

HEADERS += \
	API.h \
	ArchProcessor.h \
	ArchTypes.h \
	BasicBlock.h \
	BinaryString.h \
	ByteShiftArray.h \
	CommentServer.h \
	Configuration.h \
	DataViewInfo.h \
	Debugger.h \
	DebuggerInternal.h \
	DialogArguments.h \
	DialogAttach.h \
	DialogEditFPU.h \
	DialogEditGPR.h \
	DialogEditSIMDRegister.h \
	DialogInputBinaryString.h \
	DialogInputValue.h \
	DialogMemoryRegions.h \
	DialogOpenProgram.h \
	DialogOptions.h \
	DialogPlugins.h \
	DialogAbout.h \
	DialogThreads.h \
	Expression.h \
	FixedFontSelector.h \
	Float80Edit.h \
	FloatX.h \
	GPREdit.h \
	HexStringValidator.h \
	IAnalyzer.h \
	IBinary.h \
	IBreakpoint.h \
	IDebugEvent.h \
	IDebugEventHandler.h \
	IDebugger.h \
	IPlugin.h \
	IProcess.h \
	IThread.h \
	IRegion.h \
	IState.h \
	ISymbolManager.h \
	Instruction.h \
	LineEdit.h \
	MD5.h \
	MemoryRegions.h \
	Module.h \
	OSTypes.h \
	PluginModel.h \
	ProcessModel.h \
	Prototype.h \
	QDisassemblyView.h \
	QLongValidator.h \
	QULongValidator.h \
	RecentFileManager.h \
	RegionBuffer.h \
	Register.h \
	RegisterListWidget.h \
	RegisterViewDelegate.h \
	ShiftBuffer.h \
	State.h \
	Symbol.h \
	SymbolManager.h \
	SyntaxHighlighter.h \
	TabWidget.h \
	ThreadsModel.h \
	Types.h \
	Util.h \
	edb.h \
	string_hash.h \
	version.h \
    CallStack.h


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
	DialogThreads.ui \
	DialogAbout.ui \
	FixedFontSelector.ui

SOURCES += \
	ArchProcessor.cpp \
	BasicBlock.cpp \
	BinaryString.cpp \
	ByteShiftArray.cpp \
	CommentServer.cpp \
	Configuration.cpp \
	DataViewInfo.cpp \
	Debugger.cpp \
	DialogArguments.cpp \
	DialogAttach.cpp \
	DialogEditFPU.cpp \
	DialogEditGPR.cpp \
	DialogEditSIMDRegister.cpp \
	DialogInputBinaryString.cpp \
	DialogInputValue.cpp \
	DialogMemoryRegions.cpp \
	DialogOpenProgram.cpp \
	DialogOptions.cpp \
	DialogPlugins.cpp \
	DialogAbout.cpp \
	DialogThreads.cpp \
	FixedFontSelector.cpp \
	Float80Edit.cpp \
	FloatX.cpp \
	Function.cpp \
	GPREdit.cpp \
	HexStringValidator.cpp \
	Instruction.cpp \
	LineEdit.cpp \
	MD5.cpp \
	MemoryRegions.cpp \
	PluginModel.cpp \
	ProcessModel.cpp \
	QDisassemblyView.cpp \
	QLongValidator.cpp \
	QULongValidator.cpp \
	RecentFileManager.cpp \
	RegionBuffer.cpp \
	Register.cpp \
	RegisterListWidget.cpp \
	RegisterViewDelegate.cpp \
	State.cpp \
	SymbolManager.cpp \
	SyntaxHighlighter.cpp \
	TabWidget.cpp \
	ThreadsModel.cpp \
	edb.cpp \
	main.cpp \
    CallStack.cpp

# QHexView stuff
INCLUDEPATH += qhexview
VPATH       += qhexview
SOURCES     += qhexview.cpp
HEADERS     += qhexview.h QHexView

win32 {
	win32-msvc*:contains(QMAKE_HOST.arch, x86_64|i[3456]86) {
		VPATH       += $$LEVEL/include/os/win32 arch/x86-generic $$LEVEL/include/arch/x86-generic capstone-edb
		INCLUDEPATH += $$LEVEL/include/os/win32 arch/x86-generic $$LEVEL/include/arch/x86-generic capstone-edb "C:\\Program Files\\boost\\boost_1_51"
		DEFINES     += _CRT_SECURE_NO_WARNINGS
		RC_FILE     = edb.rc
	}
}

unix {
	LIBS += -lcapstone

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
	
	VPATH       += $$LEVEL/include/os/unix capstone-edb
	INCLUDEPATH += $$LEVEL/include/os/unix capstone-edb

	# OS include paths
	openbsd-* : INCLUDEPATH += /usr/local/include
	macx-*    : INCLUDEPATH += /opt/local/include
	

	macx {
		VPATH       += arch/x86-generic $$LEVEL/include/arch/x86-generic
		INCLUDEPATH += arch/x86-generic $$LEVEL/include/arch/x86-generic
	}
	
	*-g++-* {
		VPATH       += arch/x86-generic $$LEVEL/include/arch/x86-generic
		INCLUDEPATH += arch/x86-generic $$LEVEL/include/arch/x86-generic
	} else:!macx:contains(QMAKE_HOST.arch, x86_64|i[3456]86) {
		VPATH       += arch/x86-generic $$LEVEL/include/arch/x86-generic
		INCLUDEPATH += arch/x86-generic $$LEVEL/include/arch/x86-generic
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
	

	QMAKE_CLEAN += $${DESTDIR}/$${TARGET}
}
