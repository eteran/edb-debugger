cmake_minimum_required (VERSION 3.0)
project (edb)
include("GNUInstallDirs")

find_package(Boost 1.35 REQUIRED)

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
