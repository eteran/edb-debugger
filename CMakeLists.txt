cmake_minimum_required (VERSION 3.0)
project (edb)
include("GNUInstallDirs")

find_package(Boost 1.35 REQUIRED)

# TODO(eteran): do some arch detection and pick the right directories here
include_directories(
	"include"
	"include/arch/x86-generic"
	"include/os/unix"
)

add_subdirectory(src)
add_subdirectory(plugins)

install (FILES edb.1 DESTINATION ${CMAKE_INSTALL_MANDIR})
