cmake_minimum_required (VERSION 2.8)
project (edb CXX)

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/src/qjson4/" OR NOT EXISTS "${PROJECT_SOURCE_DIR}/src/qhexview/")
	message(SEND_ERROR "The git submodules are not available. Please run:\ngit submodule update --init --recursive"
)
endif()


if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

# if the user has not specified which version, we try to make Qt5 happen
# if they have, we prefer their preference
if (NOT QT_VERSION)
	SET(QT_VERSION "Qt5" CACHE STRING "Version of Qt to use")
	SET_PROPERTY(CACHE QT_VERSION PROPERTY STRINGS Qt5 Qt4) 
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")

include("GNUInstallDirs")

find_package(Boost 1.35 REQUIRED)

find_package(Capstone REQUIRED)
include_directories(${CAPSTONE_INCLUDE_DIRS})
link_directories(${CAPSTONE_LIBRARY_DIRS})

# eventually use this to generate version.h?
set(TARGET_VERSION_MAJOR 0)
set(TARGET_VERSION_MINOR 9)
set(TARGET_VERSION_PATCH 22)
set_property(GLOBAL PROPERTY VERSION ${TARGET_VERSION_MAJOR}.${TARGET_VERSION_MINOR}.${TARGET_VERSION_PATCH})


if (UNIX)
# TODO(eteran): how low can we make this
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GRAPHVIZ libgvc>=2.38.0)
  if(${GRAPHVIZ_FOUND})
    include_directories(${GRAPHVIZ_INCLUDE_DIRS})
    link_directories(${GRAPHVIZ_LIBRARY_DIRS})
    add_definitions(-DENABLE_GRAPH)
  endif()
endif()

if(${QT_VERSION} STREQUAL "Qt5")
	find_package(Qt5Core)
endif()

include_directories("include")

if(UNIX)
	include_directories("include/os/unix")
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	include_directories("include/os/unix/linux")
endif()

if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
	add_definitions(-DEDB_IS_32_BIT=true -DEDB_IS_64_BIT=false)
elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
	add_definitions(-DEDB_IS_32_BIT=false -DEDB_IS_64_BIT=true)
else()
	message(SEND_ERROR "Unexpected bitness: \"sizeof(void*)=${CMAKE_SIZEOF_VOID_P}.\"")
endif()

if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "i[3456]86") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64"))
	if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
		add_definitions(-DEDB_X86)
	elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
		add_definitions(-DEDB_X86_64)
	endif()
	include_directories("include/arch/x86-generic")
endif()

if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv[0-9]+"))
	if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
		add_definitions(-DEDB_ARM32)
	elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
		add_definitions(-DEDB_ARM64)
	endif()
	include_directories("include/arch/arm-generic")
endif()

if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -pedantic -Wunused")
	set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -W -Wall -pedantic -Wunused")
endif()


add_subdirectory(src)
add_subdirectory(plugins)

install (FILES ${CMAKE_SOURCE_DIR}/edb.1 DESTINATION ${CMAKE_INSTALL_MANDIR})
install (FILES ${CMAKE_SOURCE_DIR}/edb.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications/)
install (FILES ${CMAKE_SOURCE_DIR}/src/images/edb.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pixmaps/)
