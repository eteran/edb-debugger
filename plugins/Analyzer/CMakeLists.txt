cmake_minimum_required (VERSION 3.0)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(PluginName "Analyzer")

if(Qt5Core_FOUND)
    find_package(Qt5 5.0.0 REQUIRED Widgets Concurrent)
else(Qt5Core_FOUND)
	find_package(Qt4 4.6.0 QUIET REQUIRED QtGui)
endif()

# we put the header files from the include directory here 
# too so automoc can "just work"
add_library(${PluginName} SHARED
	Analyzer.h
	AnalyzerWidget.h
	OptionsPage.h
	SpecifiedFunctions.h
	Analyzer.cpp
	AnalyzerWidget.cpp
	OptionsPage.cpp
	SpecifiedFunctions.cpp
)

if(Qt5Core_FOUND)
	target_link_libraries(${PluginName} Qt5::Widgets Qt5::Concurrent)
else(Qt5Core_FOUND)
	target_link_libraries(${PluginName} Qt4::QtGui)
endif()

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)
