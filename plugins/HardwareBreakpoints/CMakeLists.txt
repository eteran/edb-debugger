cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "HardwareBreakpoints")

find_package(Qt5 REQUIRED Widgets)

add_library(${PLUGIN_NAME} SHARED
	DialogHwBreakpoints.cpp
	DialogHwBreakpoints.h
	DialogHwBreakpoints.ui
	HardwareBreakpoints.cpp
	HardwareBreakpoints.h
	libHardwareBreakpoints.cpp
	libHardwareBreakpoints.h
)

target_link_libraries(${PLUGIN_NAME} Qt${QT_VERSION_MAJOR}::Widgets edb)

build_plugin(${PLUGIN_NAME})
