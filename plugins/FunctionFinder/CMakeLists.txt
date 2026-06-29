cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "FunctionFinder")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED Widgets)

add_library(${PLUGIN_NAME} SHARED
	DialogFunctions.cpp
	DialogFunctions.h
	DialogFunctions.ui
	DialogResults.cpp
	DialogResults.h
	DialogResults.ui
	FunctionFinder.cpp
	FunctionFinder.h
	ResultsModel.h
	ResultsModel.cpp
)

target_link_libraries(${PLUGIN_NAME} Qt::Widgets edb)

build_plugin(${PLUGIN_NAME})
