cmake_minimum_required (VERSION 2.8)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
include("${PROJECT_SOURCE_DIR}/cmake/EnableCXX14.cmake")
set(PluginName "DebuggerErrorConsole")

find_package(Qt5 5.0.0 REQUIRED Widgets)

# we put the header files from the include directory here
# too so automoc can "just work"
add_library(${PluginName} SHARED
	Plugin.cpp
	Plugin.h
)

target_link_libraries(${PluginName} Qt5::Widgets)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)
