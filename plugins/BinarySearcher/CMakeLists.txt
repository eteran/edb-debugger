cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "BinarySearcher")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED Widgets)

add_library(${PLUGIN_NAME} SHARED
	BinarySearcher.cpp
	BinarySearcher.h
	DialogAsciiString.cpp
	DialogAsciiString.h
	DialogAsciiString.ui
	DialogBinaryString.cpp
	DialogBinaryString.h
	DialogBinaryString.ui
	DialogResults.cpp
	DialogResults.h
	DialogResults.ui
)

target_link_libraries(${PLUGIN_NAME} Qt::Widgets edb)

build_plugin(${PLUGIN_NAME})
