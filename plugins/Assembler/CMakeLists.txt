cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "Assembler")

find_package(Qt5 REQUIRED Widgets Xml)

qt5_add_resources(QRC_SOURCES
	Assembler.qrc
)

foreach(FILE IN LISTS QRC_SOURCES)
	set_property(SOURCE ${FILE} PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endforeach()

add_library(${PLUGIN_NAME} SHARED
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

target_link_libraries(${PLUGIN_NAME} Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Xml edb)

build_plugin(${PLUGIN_NAME})
