cmake_minimum_required (VERSION 3.1)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(PluginName "DumpState")

find_package(Qt5 5.0.0 REQUIRED Widgets)

add_library(${PluginName} SHARED
	DumpState.cpp
	DumpState.h
	OptionsPage.cpp
	OptionsPage.h
	OptionsPage.ui
)

target_link_libraries(${PluginName} Qt5::Widgets edb)

install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

set_property(TARGET ${PluginName} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PluginName} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_property(TARGET ${PluginName} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
