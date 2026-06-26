cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "Backtrace")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED Widgets)

add_library(${PLUGIN_NAME} SHARED
	Backtrace.cpp
	Backtrace.h
	CallStack.cpp
	CallStack.h
	DialogBacktrace.cpp
	DialogBacktrace.h
	DialogBacktrace.ui
)

target_link_libraries(${PLUGIN_NAME} Qt${QT_VERSION_MAJOR}::Widgets edb)

build_plugin(${PLUGIN_NAME})
