cmake_minimum_required (VERSION 3.1)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(PluginName "BinaryInfo")

find_package(Qt5 5.0.0 REQUIRED Widgets)

add_library(${PluginName} SHARED
	BinaryInfo.cpp
	BinaryInfo.h
	demangle.h
	DialogRegions.cpp
	DialogRegions.h
	DialogRegions.ui
	DialogHeader.ui
	DialogHeader.cpp
	DialogHeader.h
	ELF32.cpp
	ELF64.cpp
	ELFXX.cpp
	ELFXX.h
	OptionsPage.cpp
	OptionsPage.h
	OptionsPage.ui
	PE32.cpp
	PE32.h
	symbols.cpp
	symbols.h
)

target_link_libraries(${PluginName} Qt5::Widgets PE ELF edb)

install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

set_property(TARGET ${PluginName} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PluginName} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_property(TARGET ${PluginName} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

