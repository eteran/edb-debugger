cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "BinaryInfo")

find_package(Qt5 REQUIRED Widgets)

add_library(${PLUGIN_NAME} SHARED
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

target_link_libraries(${PLUGIN_NAME} Qt5::Widgets PE ELF edb)

build_plugin(${PLUGIN_NAME})
