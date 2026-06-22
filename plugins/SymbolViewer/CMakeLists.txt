cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "SymbolViewer")

find_package(Qt5 REQUIRED Widgets)

add_library(${PLUGIN_NAME} SHARED
	DialogSymbolViewer.cpp
	DialogSymbolViewer.h
	DialogSymbolViewer.ui
	SymbolViewer.cpp
	SymbolViewer.h
)

target_link_libraries(${PLUGIN_NAME} Qt5::Widgets edb)

build_plugin(${PLUGIN_NAME})
