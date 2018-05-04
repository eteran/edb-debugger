cmake_minimum_required (VERSION 2.8)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
include("${PROJECT_SOURCE_DIR}/cmake/EnableCXX14.cmake")
set(PluginName "Analyzer")

set(UI_FILES
		OptionsPage.ui
		SpecifiedFunctions.ui
		DialogXRefs.ui)

find_package(Qt5 5.0.0 REQUIRED Widgets Concurrent)
qt5_wrap_ui(UI_H ${UI_FILES})

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
	DialogXRefs.cpp
	DialogXRefs.h
	${UI_H}
)

target_link_libraries(${PluginName} Qt5::Widgets Qt5::Concurrent)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)
