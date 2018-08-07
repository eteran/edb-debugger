cmake_minimum_required (VERSION 3.0)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(PluginName "DebuggerCore")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt5 5.0.0 REQUIRED Widgets)

set(DebuggerCore_SRCS
	DebuggerCoreBase.cpp
	DebuggerCoreBase.h
)

if(UNIX)
	include_directories(
		"unix"
	)
	
	set(DebuggerCore_SRCS
		${DebuggerCore_SRCS}
		unix/DebuggerCoreUNIX.cpp
		unix/DebuggerCoreUNIX.h		
	)			
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

	include_directories(
		"unix/linux"
	)

	set(DebuggerCore_SRCS
		${DebuggerCore_SRCS}
		unix/linux/DebuggerCore.cpp
		unix/linux/DebuggerCore.h
		unix/linux/DialogMemoryAccess.cpp
		unix/linux/DialogMemoryAccess.h
		unix/linux/DialogMemoryAccess.ui
		unix/linux/FeatureDetect.cpp
		unix/linux/FeatureDetect.h
		unix/linux/PlatformCommon.cpp
		unix/linux/PlatformCommon.h
		unix/linux/PlatformEvent.cpp
		unix/linux/PlatformEvent.h
		unix/linux/PlatformProcess.cpp
		unix/linux/PlatformProcess.h
		unix/linux/PlatformRegion.cpp
		unix/linux/PlatformRegion.h
		unix/linux/PlatformThread.cpp
		unix/linux/PlatformThread.h	
	)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

	include_directories(
		"win32"
	)

	set(DebuggerCore_SRCS
		${DebuggerCore_SRCS}
		win32/DebuggerCore.cpp
		win32/DebuggerCore.h
		win32/PlatformEvent.cpp
		win32/PlatformEvent.h
		win32/PlatformProcess.cpp
		win32/PlatformProcess.h
		win32/PlatformRegion.cpp
		win32/PlatformRegion.h
		win32/PlatformState.cpp
		win32/PlatformState.h
	)
endif()

if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "i[3456]86") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64"))
	include_directories("arch/x86-generic")
	
	set(DebuggerCore_SRCS
		${DebuggerCore_SRCS}
		arch/x86-generic/Breakpoint.cpp
		arch/x86-generic/Breakpoint.h
	)

    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        include_directories("unix/linux/arch/x86-generic")

        set(DebuggerCore_SRCS
            ${DebuggerCore_SRCS}
            unix/linux/arch/x86-generic/PlatformState.cpp
            unix/linux/arch/x86-generic/PlatformState.h
            unix/linux/arch/x86-generic/PlatformThread.cpp
        )
    endif()

endif()

if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv[0-9]+"))
        include_directories("arch/arm-generic")

        set(DebuggerCore_SRCS
                ${DebuggerCore_SRCS}
                arch/arm-generic/Breakpoint.cpp
                arch/arm-generic/Breakpoint.h
        )

        if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
            include_directories("unix/linux/arch/arm-generic")

            set(DebuggerCore_SRCS
                ${DebuggerCore_SRCS}
                unix/linux/arch/arm-generic/PlatformState.cpp
                unix/linux/arch/arm-generic/PlatformState.h
                unix/linux/arch/arm-generic/PlatformThread.cpp
            )
        endif()

endif()

add_library(${PluginName} SHARED ${DebuggerCore_SRCS} )

target_link_libraries(${PluginName} Qt5::Widgets)

add_definitions(-DQT_PLUGIN)
target_link_libraries(${PluginName} edb)

set(LIBRARY_OUTPUT_PATH    ${PROJECT_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

set_property(TARGET ${PluginName} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD 14)
