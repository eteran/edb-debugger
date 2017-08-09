cmake_minimum_required (VERSION 2.8)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
include("${PROJECT_SOURCE_DIR}/cmake/EnableCXX11.cmake")
set(PluginName "DebuggerCore")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

set(UI_FILES
	unix/linux/DialogMemoryAccess.ui
)

endif()

if(Qt5Core_FOUND)
    find_package(Qt5 5.0.0 REQUIRED Widgets)
	qt5_wrap_ui(UI_H ${UI_FILES})
else(Qt5Core_FOUND)
	find_package(Qt4 4.6.0 QUIET REQUIRED QtCore QtGui)
	include(${QT_USE_FILE})
	qt4_wrap_ui(UI_H ${UI_FILES})
endif()


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
		unix/linux/FeatureDetect.cpp
		unix/linux/FeatureDetect.h
		unix/linux/DialogMemoryAccess.cpp
		unix/linux/DialogMemoryAccess.h
		${UI_H}
	)
endif()

if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "i[3456]86") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64"))
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

if(Qt5Core_FOUND)
	target_link_libraries(${PluginName} Qt5::Widgets)
else(Qt5Core_FOUND)
	target_link_libraries(${PluginName} Qt4::QtGui)
endif()

set(LIBRARY_OUTPUT_PATH    ${PROJECT_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)
