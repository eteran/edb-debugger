cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "HeapAnalyzer")

find_package(Qt5 REQUIRED Widgets Concurrent)

add_library(${PLUGIN_NAME} SHARED
	DialogHeap.cpp
	DialogHeap.h
	DialogHeap.ui
	HeapAnalyzer.cpp
	HeapAnalyzer.h
	ResultViewModel.cpp
	ResultViewModel.h
)

target_link_libraries(${PLUGIN_NAME} Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Concurrent edb)

build_plugin(${PLUGIN_NAME})
