cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "BreakpointManager")

find_package(Qt5 REQUIRED Widgets)

add_library(${PLUGIN_NAME} SHARED
	BreakpointManager.cpp
	BreakpointManager.h
)

target_link_libraries(${PLUGIN_NAME} Qt${QT_VERSION_MAJOR}::Widgets edb)

build_plugin(${PLUGIN_NAME})
