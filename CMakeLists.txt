cmake_minimum_required (VERSION 3.0)
project (edb)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)

find_package(Boost 1.35 REQUIRED)
find_package(Qt5Core)
if(Qt5Core_DIR)
    find_package(Qt5 5.0.0 REQUIRED Widgets Xml XmlPatterns)
elseif(NOT Qt5Core_DIR)
	find_package(Qt4 4.6.0 REQUIRED QtGui QtXml QtXmlPatterns)
endif()

add_definitions(-DDEFAULT_PLUGIN_PATH=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/edb)

include_directories(
	"include"
	"include/arch/x86-generic"
	"include/os/unix"
	"src"
	"src/capstone-edb"
	"src/qhexview"
	"src/widgets"
)

# we put the header files from the include directory here 
# too so automoc can "just work"
add_executable(edb
	include/API.h
	include/ArchProcessor.h
	include/arch/x86-generic/ArchTypes.h
	include/arch/x86-generic/Instruction.h
	include/BasicBlock.h
	include/BinaryString.h
	include/ByteShiftArray.h
	include/CallStack.h
	include/Configuration.h
	include/edb.h
	include/Expression.h
	include/FloatX.h
	include/Function.h
	include/IAnalyzer.h
	include/IBinary.h
	include/IBreakpoint.h
	include/IDebugEvent.h
	include/IDebugEventHandler.h
	include/IDebugger.h
	include/IPlugin.h
	include/IProcess.h
	include/IRegion.h
	include/IState.h
	include/ISymbolGenerator.h
	include/ISymbolManager.h
	include/IThread.h
	include/MemoryRegions.h
	include/Module.h
	include/os/unix/OSTypes.h
	include/os/win32/OSTypes.h
	include/Prototype.h
	include/Register.h
	include/ShiftBuffer.h
	include/State.h
	include/string_hash.h
	include/Symbol.h
	include/ThreadsModel.h
	include/Types.h
	include/version.h
	src/arch/x86-generic/ArchProcessor.cpp
	src/arch/x86-generic/DialogEditFPU.cpp
	src/arch/x86-generic/DialogEditGPR.cpp
	src/arch/x86-generic/DialogEditSIMDRegister.cpp
	src/arch/x86-generic/Float80Edit.cpp
	src/arch/x86-generic/GPREdit.cpp
	src/BasicBlock.cpp
	src/BinaryString.cpp
	src/ByteShiftArray.cpp
	src/CallStack.cpp
	src/capstone-edb/Instruction.cpp
	src/CommentServer.cpp
	src/Configuration.cpp
	src/DataViewInfo.cpp
	src/Debugger.cpp
	src/debugger.qrc
	src/DialogAbout.cpp
	src/DialogArguments.cpp
	src/DialogAttach.cpp
	src/DialogInputBinaryString.cpp
	src/DialogInputValue.cpp
	src/DialogMemoryRegions.cpp
	src/DialogOpenProgram.cpp
	src/DialogOptions.cpp
	src/DialogPlugins.cpp
	src/DialogThreads.cpp
	src/edb.cpp
	src/FixedFontSelector.cpp
	src/FloatX.cpp
	src/Function.cpp
	src/HexStringValidator.cpp
	src/main.cpp
	src/MD5.cpp
	src/MemoryRegions.cpp
	src/PluginModel.cpp
	src/ProcessModel.cpp
	src/qhexview/qhexview.cpp
	src/QLongValidator.cpp
	src/QULongValidator.cpp
	src/RecentFileManager.cpp
	src/RegionBuffer.cpp
	src/Register.cpp
	src/State.cpp
	src/SymbolManager.cpp
	src/ThreadsModel.cpp
	src/widgets/LineEdit.cpp
	src/widgets/QDisassemblyView.cpp
	src/widgets/RegisterListWidget.cpp
	src/widgets/RegisterViewDelegate.cpp
	src/widgets/SyntaxHighlighter.cpp
	src/widgets/TabWidget.cpp
)

if(Qt5Core_DIR)
	target_link_libraries(edb capstone Qt5::Widgets Qt5::Xml Qt5::XmlPatterns)
elseif(NOT Qt5Core_DIR)
	target_link_libraries(edb capstone Qt4::QtGui Qt4::QtXml Qt4::QtXmlPatterns)
endif()



add_subdirectory(plugins)
install (TARGETS edb DESTINATION ${CMAKE_INSTALL_BINDIR})
install (FILES edb.1 DESTINATION ${CMAKE_INSTALL_MANDIR})
