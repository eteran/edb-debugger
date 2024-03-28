cmake_minimum_required (VERSION 3.15)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(PLUGIN_NAME "CheckVersion")

find_package(Qt5 5.0.0 REQUIRED Widgets Network)

add_library(${PLUGIN_NAME} SHARED
	CheckVersion.cpp
	CheckVersion.h
	OptionsPage.cpp
	OptionsPage.h
	OptionsPage.ui
)

target_link_libraries(${PLUGIN_NAME} Qt5::Widgets Qt5::Network edb)

install (TARGETS ${PLUGIN_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

target_add_warnings(${PLUGIN_NAME})

set_target_properties(${PLUGIN_NAME}
    PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
	LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)
