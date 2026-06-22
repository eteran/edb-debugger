cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "CheckVersion")

find_package(Qt5 REQUIRED Widgets Network)

add_library(${PLUGIN_NAME} SHARED
	CheckVersion.cpp
	CheckVersion.h
	OptionsPage.cpp
	OptionsPage.h
	OptionsPage.ui
)

target_link_libraries(${PLUGIN_NAME} Qt5::Widgets Qt5::Network edb)

build_plugin(${PLUGIN_NAME})
