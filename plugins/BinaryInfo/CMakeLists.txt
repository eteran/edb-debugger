cmake_minimum_required (VERSION 3.0)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(PluginName "BinaryInfo")

if(Qt5Core_FOUND)
    find_package(Qt5 5.0.0 REQUIRED Widgets)
else(Qt5Core_FOUND)
	find_package(Qt4 4.6.0 QUIET REQUIRED QtGui)
endif()

# we put the header files from the include directory here 
# too so automoc can "just work"
add_library(${PluginName} SHARED
	BinaryInfo.cpp
	BinaryInfo.h
	demangle.h
	DialogHeader.cpp
	DialogHeader.h
	ELF32.cpp
	ELF32.h
	ELF64.cpp
	ELF64.h
	elf_binary.h
	OptionsPage.cpp
	OptionsPage.h
	PE32.cpp
	PE32.h
	pe_binary.h
	symbols.cpp
	symbols.h
)

if(Qt5Core_FOUND)
	target_link_libraries(${PluginName} Qt5::Widgets)
else(Qt5Core_FOUND)
	target_link_libraries(${PluginName} Qt4::QtGui)
endif()

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)
