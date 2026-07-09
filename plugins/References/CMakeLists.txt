cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "References")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED Widgets Concurrent)

add_library(${PLUGIN_NAME} SHARED
	DialogReferences.cpp
	DialogReferences.h
	DialogReferences.ui
	References.cpp
	References.h
)

target_link_libraries(${PLUGIN_NAME} Qt::Widgets Qt::Concurrent edb)

build_plugin(${PLUGIN_NAME})
