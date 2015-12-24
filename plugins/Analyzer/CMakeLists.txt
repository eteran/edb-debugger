cmake_minimum_required (VERSION 3.0)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)

find_package(Qt5Core)

if(Qt5Core_FOUND)
    find_package(Qt5 5.0.0 REQUIRED Widgets Concurrent)
else(Qt5Core_FOUND)
	find_package(Qt4 4.6.0 REQUIRED QtGui QtConcurrent)
endif()

include_directories(
	"${PROJECT_SRC_DIR}/include"
)

# we put the header files from the include directory here 
# too so automoc can "just work"
add_library(Analyzer SHARED
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
	target_link_libraries(Analyzer Qt5::Widgets Qt5::Concurrent)
else(Qt5Core_FOUND)
	target_link_libraries(Analyzer Qt4::QtGui Qt4::QtConcurrent)
endif()

set(LIBRARY_OUTPUT_PATHAnalyzer${PROJECT_BINARY_DIR})
install (TARGETS Analyzer DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)
