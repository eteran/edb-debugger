cmake_minimum_required (VERSION 2.8)
include("GNUInstallDirs")
include("${PROJECT_SOURCE_DIR}/cmake/EnableCXX14.cmake")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(UI_FILES
		BinaryString.ui
		Debugger.ui
		DialogAbout.ui
		DialogArguments.ui
		DialogAttach.ui
		DialogInputBinaryString.ui
		DialogInputValue.ui
		DialogMemoryRegions.ui
		DialogOptions.ui
		FixedFontSelector.ui
		DialogPlugins.ui
		DialogThreads.ui)

set(RC_FILES debugger.qrc)


find_package(Qt5 5.0.0 REQUIRED Widgets Xml XmlPatterns Svg)
qt5_wrap_ui(UI_H ${UI_FILES})
qt5_add_resources(RC_SRCS ${RC_FILES})

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
	execute_process(
		COMMAND git rev-parse HEAD
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_BRANCH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
else()
	set(GIT_BRANCH "Unknown")
endif()
add_definitions(-DGIT_BRANCH=${GIT_BRANCH})
add_definitions(-DDEFAULT_PLUGIN_PATH=\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/edb\")

include_directories(
	"capstone-edb"
	"qhexview"
	"widgets"
	"qjson4"
	"session"
)

set(edb_SRCS
	BasicBlock.cpp
	BinaryString.cpp
	ByteShiftArray.cpp
	capstone-edb/Instruction.cpp
	capstone-edb/Inspection.cpp
	CommentServer.cpp
	Configuration.cpp
	DataViewInfo.cpp
	DebugEventHandlers.cpp
	Debugger.cpp
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
	ExpressionDialog.cpp
	FixedFontSelector.cpp
	FloatX.cpp
	Function.cpp
	HexStringValidator.cpp
	main.cpp
	MemoryRegions.cpp
	PluginModel.cpp
	ProcessModel.cpp
	qhexview/qhexview.cpp
	QLongValidator.cpp
	QULongValidator.cpp
	RecentFileManager.cpp
	RegionBuffer.cpp
	Register.cpp
	RegisterViewModelBase.cpp
	State.cpp
	SymbolManager.cpp
	session/SessionManager.cpp
	session/SessionError.cpp
	ThreadsModel.cpp
	widgets/LineEdit.cpp
	widgets/NavigationHistory.cpp
	widgets/QDisassemblyView.cpp
	widgets/RegisterViewDelegate.cpp
	widgets/SyntaxHighlighter.cpp
	widgets/TabWidget.cpp
	qjson4/QJsonArray.cpp
	qjson4/QJsonDocument.cpp
	qjson4/QJsonObject.cpp
	qjson4/QJsonParseError.cpp
	qjson4/QJsonParser.cpp
	qjson4/QJsonValue.cpp
	qjson4/QJsonValueRef.cpp
	${RC_SRCS}
	${UI_H}
)

set(edb_INCLUDES
	${PROJECT_SOURCE_DIR}/include/API.h
	${PROJECT_SOURCE_DIR}/include/ArchProcessor.h
	${PROJECT_SOURCE_DIR}/include/BasicBlock.h
	${PROJECT_SOURCE_DIR}/include/BinaryString.h
	${PROJECT_SOURCE_DIR}/include/ByteShiftArray.h
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
	${PROJECT_SOURCE_DIR}/include/RegisterViewModelBase.h
	${PROJECT_SOURCE_DIR}/include/ShiftBuffer.h
	${PROJECT_SOURCE_DIR}/include/State.h
	${PROJECT_SOURCE_DIR}/include/string_hash.h
	${PROJECT_SOURCE_DIR}/include/Symbol.h
	${PROJECT_SOURCE_DIR}/include/ThreadsModel.h
	${PROJECT_SOURCE_DIR}/include/Types.h
	${PROJECT_SOURCE_DIR}/include/version.h
	${PROJECT_SOURCE_DIR}/include/QLongValidator.h
	${PROJECT_SOURCE_DIR}/include/QULongValidator.h
	${PROJECT_SOURCE_DIR}/include/HexStringValidator.h
	${PROJECT_SOURCE_DIR}/include/Instruction.h
	qjson4/QJsonArray.h
	qjson4/QJsonDocument.h
	qjson4/QJsonObject.h
	qjson4/QJsonParseError.h
	qjson4/QJsonParser.h
	qjson4/QJsonRoot.h
	qjson4/QJsonValue.h
	qjson4/QJsonValueRef.h
)

if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "i[3456]86") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64"))
	set(edb_INCLUDES
		${edb_INCLUDES}
		${PROJECT_SOURCE_DIR}/include/arch/x86-generic/ArchTypes.h
	)

	set(edb_SRCS
		${edb_SRCS}
		arch/x86-generic/ArchProcessor.cpp
		arch/x86-generic/RegisterViewModel.cpp
	)
endif()

if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv[0-9]+"))
        set(edb_INCLUDES
                ${edb_INCLUDES}
                ${PROJECT_SOURCE_DIR}/include/arch/arm-generic/ArchTypes.h
        )

        set(edb_SRCS
                ${edb_SRCS}
                arch/arm-generic/ArchProcessor.cpp
                arch/arm-generic/RegisterViewModel.cpp
        )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

  enable_language(ASM_MASM)
  
  set(CMAKE_ASM_MASM_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /safeseh")
  
  set(edb_SRCS
    ${edb_SRCS}
    LongDoubleX86.asm
  )
  
endif()

if(${GRAPHVIZ_FOUND})
	set(edb_INCLUDES
		${edb_INCLUDES}
		${PROJECT_SOURCE_DIR}/include/GraphWidget.h
		${PROJECT_SOURCE_DIR}/include/GraphEdge.h
		${PROJECT_SOURCE_DIR}/include/GraphNode.h
	)

	set(edb_SRCS
		${edb_SRCS}
		graph/GraphEdge.cpp
		graph/GraphicsScene.cpp
		graph/GraphicsScene.h
		graph/GraphNode.cpp
		graph/GraphWidget.cpp
	)
endif()

# we put the header files from the include directory here
# too so automoc can "just work"
add_executable(edb ${edb_INCLUDES} ${edb_SRCS})

target_link_libraries(edb ${CAPSTONE_LIBRARIES} Qt5::Widgets Qt5::Xml Qt5::XmlPatterns Qt5::Svg ${GRAPHVIZ_LIBRARIES})

set_target_properties(edb PROPERTIES ENABLE_EXPORTS TRUE)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS edb DESTINATION ${CMAKE_INSTALL_BINDIR})
