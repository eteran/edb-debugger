cmake_minimum_required (VERSION 3.0)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(PluginName "Assembler")

set(UI_FILES
		DialogAssembler.ui
		OptionsPage.ui)
set(RC_FILES Assembler.qrc)

find_package(Qt5 5.0.0 REQUIRED Widgets Xml XmlPatterns)
qt5_wrap_ui(UI_H ${UI_FILES})
qt5_add_resources(RC_SRCS ${RC_FILES})

# we put the header files from the include directory here 
# too so automoc can "just work"
add_library(${PluginName} SHARED
	Assembler.cpp
	Assembler.h
	DialogAssembler.cpp
	DialogAssembler.h
	OptionsPage.cpp
	OptionsPage.h
	${RC_SRCS}
	${UI_H}
)

target_link_libraries(${PluginName} Qt5::Widgets Qt5::Xml Qt5::XmlPatterns)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

set_property(TARGET ${PluginName} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD 14)
