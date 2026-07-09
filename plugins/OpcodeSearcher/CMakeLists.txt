cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "OpcodeSearcher")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED Widgets Concurrent)

add_library(${PLUGIN_NAME} SHARED
	DialogOpcodes.cpp
	DialogOpcodes.h
	DialogOpcodes.ui
	DialogResults.cpp
	DialogResults.h
	DialogResults.ui
	OpcodeSearcher.cpp
	OpcodeSearcher.h
	ResultsModel.cpp
	ResultsModel.h
)

target_link_libraries(${PLUGIN_NAME} Qt::Widgets Qt::Concurrent edb)

build_plugin(${PLUGIN_NAME})
