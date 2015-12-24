cmake_minimum_required (VERSION 3.0)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)


find_package(Qt5Core)
if(Qt5Core_DIR)
    find_package(Qt5 5.0.0 REQUIRED Widgets Xml XmlPatterns)
elseif(NOT Qt5Core_DIR)
	find_package(Qt4 4.6.0 REQUIRED QtGui QtXml QtXmlPatterns)
endif()

add_definitions(-DDEFAULT_PLUGIN_PATH=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/edb)


# TODO(eteran): do some arch detection and pick the right directories here
include_directories(
	"capstone-edb"
	"qhexview"
	"widgets"
)

set(edb_SRCS
	arch/x86-generic/ArchProcessor.cpp
	arch/x86-generic/DialogEditFPU.cpp
	arch/x86-generic/DialogEditGPR.cpp
	arch/x86-generic/DialogEditSIMDRegister.cpp
	arch/x86-generic/Float80Edit.cpp
	arch/x86-generic/GPREdit.cpp
	BasicBlock.cpp
	BinaryString.cpp
	ByteShiftArray.cpp
	CallStack.cpp
	capstone-edb/Instruction.cpp
	CommentServer.cpp
	Configuration.cpp
	DataViewInfo.cpp
	Debugger.cpp
	debugger.qrc
	DialogAbout.cpp
	DialogArguments.cpp
	DialogAttach.cpp
	DialogInputBinaryString.cpp
	DialogInputValue.cpp
	DialogMemoryRegions.cpp
	DialogOpenProgram.cpp
	DialogOptions.cpp
	DialogPlugins.cpp
	DialogThreads.cpp
	edb.cpp
	FixedFontSelector.cpp
	FloatX.cpp
	Function.cpp
	HexStringValidator.cpp
	main.cpp
	MD5.cpp
	MemoryRegions.cpp
	PluginModel.cpp
	ProcessModel.cpp
	qhexview/qhexview.cpp
	QLongValidator.cpp
	QULongValidator.cpp
	RecentFileManager.cpp
	RegionBuffer.cpp
	Register.cpp
	State.cpp
	SymbolManager.cpp
	ThreadsModel.cpp
	widgets/LineEdit.cpp
	widgets/QDisassemblyView.cpp
	widgets/RegisterListWidget.cpp
	widgets/RegisterViewDelegate.cpp
	widgets/SyntaxHighlighter.cpp
	widgets/TabWidget.cpp
)

set(edb_INCLUDES
	${PROJECT_SOURCE_DIR}/include/API.h
	${PROJECT_SOURCE_DIR}/include/ArchProcessor.h
	${PROJECT_SOURCE_DIR}/include/arch/x86-generic/ArchTypes.h
	${PROJECT_SOURCE_DIR}/include/arch/x86-generic/Instruction.h
	${PROJECT_SOURCE_DIR}/include/BasicBlock.h
	${PROJECT_SOURCE_DIR}/include/BinaryString.h
	${PROJECT_SOURCE_DIR}/include/ByteShiftArray.h
	${PROJECT_SOURCE_DIR}/include/CallStack.h
	${PROJECT_SOURCE_DIR}/include/Configuration.h
	${PROJECT_SOURCE_DIR}/include/edb.h
	${PROJECT_SOURCE_DIR}/include/Expression.h
	${PROJECT_SOURCE_DIR}/include/FloatX.h
	${PROJECT_SOURCE_DIR}/include/Function.h
	${PROJECT_SOURCE_DIR}/include/IAnalyzer.h
	${PROJECT_SOURCE_DIR}/include/IBinary.h
	${PROJECT_SOURCE_DIR}/include/IBreakpoint.h
	${PROJECT_SOURCE_DIR}/include/IDebugEvent.h
	${PROJECT_SOURCE_DIR}/include/IDebugEventHandler.h
	${PROJECT_SOURCE_DIR}/include/IDebugger.h
	${PROJECT_SOURCE_DIR}/include/IPlugin.h
	${PROJECT_SOURCE_DIR}/include/IProcess.h
	${PROJECT_SOURCE_DIR}/include/IRegion.h
	${PROJECT_SOURCE_DIR}/include/IState.h
	${PROJECT_SOURCE_DIR}/include/ISymbolGenerator.h
	${PROJECT_SOURCE_DIR}/include/ISymbolManager.h
	${PROJECT_SOURCE_DIR}/include/IThread.h
	${PROJECT_SOURCE_DIR}/include/MemoryRegions.h
	${PROJECT_SOURCE_DIR}/include/Module.h
	${PROJECT_SOURCE_DIR}/include/os/unix/OSTypes.h
	${PROJECT_SOURCE_DIR}/include/os/win32/OSTypes.h
	${PROJECT_SOURCE_DIR}/include/Prototype.h
	${PROJECT_SOURCE_DIR}/include/Register.h
	${PROJECT_SOURCE_DIR}/include/ShiftBuffer.h
	${PROJECT_SOURCE_DIR}/include/State.h
	${PROJECT_SOURCE_DIR}/include/string_hash.h
	${PROJECT_SOURCE_DIR}/include/Symbol.h
	${PROJECT_SOURCE_DIR}/include/ThreadsModel.h
	${PROJECT_SOURCE_DIR}/include/Types.h
	${PROJECT_SOURCE_DIR}/include/version.h
)

# we put the header files from the include directory here 
# too so automoc can "just work"
add_executable(edb ${edb_INCLUDES} ${edb_SRCS})

if(Qt5Core_DIR)
	target_link_libraries(edb capstone Qt5::Widgets Qt5::Xml Qt5::XmlPatterns)
elseif(NOT Qt5Core_DIR)
	target_link_libraries(edb capstone Qt4::QtGui Qt4::QtXml Qt4::QtXmlPatterns)
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS edb DESTINATION ${CMAKE_INSTALL_BINDIR})

