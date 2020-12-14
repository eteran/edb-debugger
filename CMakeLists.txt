cmake_minimum_required (VERSION 3.1)
project (edb CXX)

enable_testing()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/src/qhexview/.git")
	message(SEND_ERROR "The git submodules are not available. Please run:\ngit submodule update --init --recursive")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")

include("GNUInstallDirs")
include("CheckIncludeFileCXX")
include("DetectCompiler")
include("DetectOS")
include("DetectArchitecture")
include("EnableSanitizers")
include("EnableSTLDebug")
include("ProjectDefaults")

if(TARGET_COMPILER_GCC AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 5.0)
	message(FATAL_ERROR "Your g++ version is too old. At least 5.0 is required.")
endif()

find_package(Boost 1.35 REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

find_package(Capstone REQUIRED)
include_directories(${CAPSTONE_INCLUDE_DIRS})
link_directories(${CAPSTONE_LIBRARY_DIRS})

# eventually use this to generate version.h?
set(TARGET_VERSION_MAJOR 1)
set(TARGET_VERSION_MINOR 3)
set(TARGET_VERSION_PATCH 0)
set_property(GLOBAL PROPERTY VERSION ${TARGET_VERSION_MAJOR}.${TARGET_VERSION_MINOR}.${TARGET_VERSION_PATCH})

if (UNIX)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GRAPHVIZ libgvc>=2.38.0)
	if(GRAPHVIZ_FOUND)
		include_directories(${GRAPHVIZ_INCLUDE_DIRS})
		link_directories(${GRAPHVIZ_LIBRARY_DIRS})
		add_definitions(-DENABLE_GRAPH)
	endif()
endif()

message(STATUS "Checking for module 'double-conversion'")
# Using check_include_file_cxx instead of find_package etc. to support various versions of double-conversion. Some versions don't have CMake modules, some have a bit incompatible ones...
check_include_file_cxx("double-conversion/double-conversion.h" HAVE_DOUBLE_CONVERSION)
if(NOT HAVE_DOUBLE_CONVERSION)
   UNSET(HAVE_DOUBLE_CONVERSION CACHE)
   message(WARNING "libdouble-conversion header wasn't found. 32- and 64-bit floating-point values will be shown with max_digits10 digits of precision instead of shortest representation.")
else()
   find_library(DOUBLE_CONVERSION_LIBRARIES double-conversion)
   if(NOT DOUBLE_CONVERSION_LIBRARIES)
	   message(WARNING "libdouble-conversion library wasn't found. 32- and 64-bit floating-point values will be shown with max_digits10 digits of precision instead of shortest representation.")
   else()
       add_definitions("-DHAVE_DOUBLE_CONVERSION")
   endif()
endif()

find_package(Qt5Core)

include_directories("include")

if(TARGET_PLATFORM_LINUX)
	include_directories("include/os/unix")
	include_directories("include/os/unix/linux")
elseif(TARGET_PLATFORM_WINDOWS)
	include_directories("include/os/win32")
endif()

if(TARGET_ARCH_X86 OR TARGET_ARCH_ARM32)
	add_definitions(-DEDB_IS_32_BIT=true -DEDB_IS_64_BIT=false)
elseif(TARGET_ARCH_X64 OR TARGET_ARCH_ARM64)
	add_definitions(-DEDB_IS_32_BIT=false -DEDB_IS_64_BIT=true)
else()
	message(SEND_ERROR "Unexpected bitness: \"sizeof(void*)=${CMAKE_SIZEOF_VOID_P}.\"")
endif()

if(TARGET_ARCH_X86)
	add_definitions(-DEDB_X86)
	include_directories("include/arch/x86-generic")
elseif(TARGET_ARCH_X64)
	add_definitions(-DEDB_X86_64)
	include_directories("include/arch/x86-generic")
elseif(TARGET_ARCH_ARM32)
	add_definitions(-DEDB_ARM32)
	include_directories("include/arch/arm-generic")
elseif(TARGET_ARCH_ARM64)
	add_definitions(-DEDB_ARM64)
	include_directories("include/arch/arm-generic")
endif()

# FIXME: This is also useful on Windows, so it should be made usable there too.
if(UNIX)
	if(TARGET_ARCH_FAMILY_X86)
		pkg_check_modules(GDTOA gdtoa-desktop)
		if(NOT GDTOA_FOUND)
			message(WARNING "gdtoa-desktop package wasn't found. 80-bit floating-point values will be shown with max_digits10 digits of precision instead of shortest representation.")
		else()
			add_definitions("-DHAVE_GDTOA")
			include_directories(${GDTOA_INCLUDE_DIRS})
			link_directories(${GDTOA_LIBRARY_DIRS})
		endif()
	endif()
endif()


if (TARGET_COMPILER_CLANG OR TARGET_COMPILER_GCC)
    add_compile_options(
		-W
		-Wall
		#-Wshadow
		-Wnon-virtual-dtor
		#-Wold-style-cast
		-Wcast-align
		-Wunused
		-Woverloaded-virtual
		-pedantic
		#-Wconversion
		#-Wsign-conversion
		#-Wnull-dereference
		-Wdouble-promotion
		-Wformat=2
		-Wno-unused-macros
		-Wno-switch-enum
		-Wno-unknown-pragmas
    )

    if(TARGET_COMPILER_CLANG)
		add_compile_options(
			#-Wshorten-64-to-32
			-Wconditional-uninitialized
			#-Wshadow-uncaptured-local
			-Wmissing-prototypes
			#-Wexit-time-destructors
			#-Wglobal-constructors
			-Wimplicit-fallthrough
		)

    elseif(TARGET_COMPILER_GCC)
		add_compile_options(
			#-Wuseless-cast
			#-Wduplicated-cond
			#-Wduplicated-branches
			-Wlogical-op

			#-Wsuggest-attribute=pure
			#-Wsuggest-attribute=const
			#-Wsuggest-attribute=noreturn
			#-Wsuggest-final-types
			#-Wsuggest-final-methods
			-Wsuggest-override
		)
	endif()
endif()

add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(libELF)
add_subdirectory(libPE)

install (FILES ${CMAKE_SOURCE_DIR}/edb.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install (FILES ${CMAKE_SOURCE_DIR}/edb.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications/)
install (FILES ${CMAKE_SOURCE_DIR}/src/images/edb.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pixmaps/)
