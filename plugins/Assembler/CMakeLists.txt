cmake_minimum_required (VERSION 3.0)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)

find_package(Qt5Core)

if(Qt5Core_FOUND)
    find_package(Qt5 5.0.0 REQUIRED Widgets Xml XmlPatterns)
else(Qt5Core_FOUND)
	find_package(Qt4 4.6.0 REQUIRED QtGui QtXml QtXmlPattern)
endif()

# we put the header files from the include directory here 
# too so automoc can "just work"
add_library(Assembler SHARED
	Assembler.cpp
	Assembler.h
	Assembler.qrc
	DialogAssembler.cpp
	DialogAssembler.h
	OptionsPage.cpp
	OptionsPage.h
)

if(Qt5Core_FOUND)
	target_link_libraries(Assembler Qt5::Widgets Qt5::Xml Qt5::XmlPatterns)
else(Qt5Core_FOUND)
	target_link_libraries(Assembler Qt4::QtXml Qt4::QtXmlPatterns)
endif()

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS Assembler DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)
