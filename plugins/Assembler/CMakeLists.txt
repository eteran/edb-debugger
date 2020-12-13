cmake_minimum_required (VERSION 3.1)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(PluginName "Assembler")

find_package(Qt5 5.0.0 REQUIRED Widgets Xml XmlPatterns)

qt5_add_resources(QRC_SOURCES 
	Assembler.qrc
)

foreach(FILE IN LISTS QRC_SOURCES)
	set_property(SOURCE ${FILE} PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endforeach()

add_library(${PluginName} SHARED
	${QRC_SOURCES}
	Assembler.cpp
	Assembler.h
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
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PluginName} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_property(TARGET ${PluginName} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
