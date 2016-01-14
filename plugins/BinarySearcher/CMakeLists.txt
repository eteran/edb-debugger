cmake_minimum_required (VERSION 2.8)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
include("${PROJECT_SOURCE_DIR}/cmake/EnableCXX11.cmake")
set(PluginName "BinarySearcher")

set(UI_FILES
		DialogASCIIString.ui
		DialogBinaryString.ui)

if(Qt5Core_FOUND)
    find_package(Qt5 5.0.0 REQUIRED Widgets)
	qt5_wrap_ui(UI_H ${UI_FILES})
else(Qt5Core_FOUND)
	find_package(Qt4 4.6.0 QUIET REQUIRED QtCore QtGui)
	include(${QT_USE_FILE})
	qt4_wrap_ui(UI_H ${UI_FILES})
endif()

# we put the header files from the include directory here 
# too so automoc can "just work"
add_library(${PluginName} SHARED
	BinarySearcher.cpp
	BinarySearcher.h
	DialogASCIIString.cpp
	DialogASCIIString.h
	DialogBinaryString.cpp
	DialogBinaryString.h
	${UI_H}
)

if(Qt5Core_FOUND)
	target_link_libraries(${PluginName} Qt5::Widgets)
else(Qt5Core_FOUND)
	target_link_libraries(${PluginName} Qt4::QtGui)
endif()

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)
