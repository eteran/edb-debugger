cmake_minimum_required (VERSION 3.0)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(PluginName "Assembler")

find_package(Qt5 5.0.0 REQUIRED Widgets Xml XmlPatterns)

add_library(${PluginName} SHARED
	Assembler.cpp
	Assembler.h
	Assembler.qrc
	DialogAssembler.cpp
	DialogAssembler.h
	DialogAssembler.ui
	OptionsPage.cpp
	OptionsPage.h
	OptionsPage.ui
)

target_link_libraries(${PluginName} Qt5::Widgets Qt5::Xml Qt5::XmlPatterns edb)

install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

set_property(TARGET ${PluginName} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${PluginName} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_property(TARGET ${PluginName} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

if (ENABLE_JUMBO_BUILD)
	enable_jumbo_build(${PluginName})
endif()
