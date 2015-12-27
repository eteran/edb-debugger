cmake_minimum_required (VERSION 3.0)

# if the user has not specified which version, we try to make Qt5 happen
# if they have, we prefer their preference
if (NOT QT_VERSION)
	SET(QT_VERSION "Qt5" CACHE STRING "Version of Qt to use")
	SET_PROPERTY(CACHE QT_VERSION PROPERTY STRINGS Qt5 Qt4) 
endif()

option(ENABLE_GRAPHVIZ "Enable Graphviz" "ON")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

project (edb)
include("GNUInstallDirs")

find_package(Boost 1.35 REQUIRED)

if(${ENABLE_GRAPHVIZ})
	find_package(GraphViz)
endif()

if(${GRAPHVIZ_FOUND})
	include_directories(${GRAPHVIZ_INCLUDE})
endif()

if(${QT_VERSION} STREQUAL "Qt5")
	find_package(Qt5Core)
endif()

include_directories("include")

if(UNIX)
	include_directories("include/os/unix")
endif()

if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "i[3456]86") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64"))
	include_directories("include/arch/x86-generic")
endif()

add_subdirectory(src)
add_subdirectory(plugins)

install (FILES edb.1 DESTINATION ${CMAKE_INSTALL_MANDIR})
