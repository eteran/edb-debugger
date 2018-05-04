cmake_minimum_required (VERSION 2.8)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
include("${PROJECT_SOURCE_DIR}/cmake/EnableCXX14.cmake")
set(PluginName "CheckVersion")

set(UI_FILES
		OptionsPage.ui)

find_package(Qt5 5.0.0 REQUIRED Widgets Network)
qt5_wrap_ui(UI_H ${UI_FILES})

# we put the header files from the include directory here 
# too so automoc can "just work"
add_library(${PluginName} SHARED
	CheckVersion.cpp
	CheckVersion.h
	OptionsPage.cpp
	OptionsPage.h
	${UI_H}
)

target_link_libraries(${PluginName} Qt5::Widgets Qt5::Network)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

set_property(TARGET ${PluginName} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD 14)
