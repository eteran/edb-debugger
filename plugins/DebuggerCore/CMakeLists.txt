cmake_minimum_required (VERSION 3.1)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(PluginName "DebuggerCore")

find_package(Qt5 5.0.0 REQUIRED Widgets)

set(DebuggerCore_SRCS
	DebuggerCoreBase.cpp
	DebuggerCoreBase.h
)

if(TARGET_PLATFORM_LINUX)

	set(PLUGIN_INCLUDES
		${PLUGIN_INCLUDES}
		unix/linux
		unix
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
		unix/linux/PrStatus.h
		unix/Posix.cpp
		unix/Posix.h
		unix/Unix.cpp
		unix/Unix.h
	)
elseif(TARGET_PLATFORM_WINDOWS)

	set(PLUGIN_INCLUDES
		${PLUGIN_INCLUDES}
		win32
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
		win32/PlatformThread.cpp
		win32/PlatformThread.h
	)
elseif(TARGET_PLATFORM_FREEBSD)

	set(PLUGIN_INCLUDES
		${PLUGIN_INCLUDES}
		unix/freebsd
		unix
	)

	set(DebuggerCore_SRCS
		${DebuggerCore_SRCS}
		unix/freebsd/DebuggerCore.cpp
		unix/freebsd/DebuggerCore.h
		unix/freebsd/PlatformCommon.cpp
		unix/freebsd/PlatformCommon.h
		unix/freebsd/PlatformEvent.cpp
		unix/freebsd/PlatformEvent.h
		unix/freebsd/PlatformProcess.cpp
		unix/freebsd/PlatformProcess.h
		unix/freebsd/PlatformRegion.cpp
		unix/freebsd/PlatformRegion.h
		unix/freebsd/PlatformThread.cpp
		unix/freebsd/PlatformThread.h
	)
endif()

if(TARGET_ARCH_FAMILY_X86)
	set(PLUGIN_INCLUDES
		${PLUGIN_INCLUDES}
		arch/x86-generic
	)
	
	set(DebuggerCore_SRCS
		${DebuggerCore_SRCS}
		arch/x86-generic/Breakpoint.cpp
		arch/x86-generic/Breakpoint.h
	)

    if(TARGET_PLATFORM_LINUX)	
		set(PLUGIN_INCLUDES
			${PLUGIN_INCLUDES}
			unix/linux/arch/x86-generic
		)

        set(DebuggerCore_SRCS
            ${DebuggerCore_SRCS}
            unix/linux/arch/x86-generic/PlatformState.cpp
            unix/linux/arch/x86-generic/PlatformState.h
            unix/linux/arch/x86-generic/PlatformThread.cpp
        )
    endif()

endif()

if(TARGET_ARCH_FAMILY_ARM)

		set(PLUGIN_INCLUDES
			${PLUGIN_INCLUDES}
			arch/arm-generic
		)

        set(DebuggerCore_SRCS
                ${DebuggerCore_SRCS}
                arch/arm-generic/Breakpoint.cpp
                arch/arm-generic/Breakpoint.h
        )

        if(TARGET_PLATFORM_LINUX)
		
			set(PLUGIN_INCLUDES
				${PLUGIN_INCLUDES}
				unix/linux/arch/arm-generic
			)

            set(DebuggerCore_SRCS
                ${DebuggerCore_SRCS}
                unix/linux/arch/arm-generic/PlatformState.cpp
                unix/linux/arch/arm-generic/PlatformState.h
                unix/linux/arch/arm-generic/PlatformThread.cpp
            )
        endif()

endif()

add_library(${PluginName} SHARED ${DebuggerCore_SRCS} )

target_include_directories(${PluginName} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${PLUGIN_INCLUDES}
)

target_link_libraries(${PluginName} Qt5::Widgets PE ELF edb)

install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

set_property(TARGET ${PluginName} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PluginName} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_property(TARGET ${PluginName} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
