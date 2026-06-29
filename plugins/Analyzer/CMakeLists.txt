cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "Analyzer")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED Widgets)

add_library(${PLUGIN_NAME} SHARED
	Analyzer.cpp
	Analyzer.h
	AnalyzerWidget.cpp
	AnalyzerWidget.h
	DialogXRefs.cpp
	DialogXRefs.h
	DialogXRefs.ui
	OptionsPage.cpp
	OptionsPage.h
	OptionsPage.ui
	SpecifiedFunctions.cpp
	SpecifiedFunctions.h
	SpecifiedFunctions.ui
)

target_link_libraries(${PLUGIN_NAME} Qt::Widgets edb)

build_plugin(${PLUGIN_NAME})
