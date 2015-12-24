cmake_minimum_required (VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)

find_package(Qt5Core)

if(Qt5Core_FOUND)
    find_package(Qt5 5.0.0 REQUIRED Widgets)
else(Qt5Core_FOUND)
	find_package(Qt4 4.6.0 REQUIRED QtGui)
endif()

include_directories(
	"${PROJECT_SRC_DIR}/include"
	"unix"
	"unix/linux"
)

# Tool for detecting broken proc/<self>/mem writes
add_executable(proc-pid-mem-write
	unix/linux/detect/proc-pid-mem-write.cpp
)

add_custom_command(
	OUTPUT ${PROJECT_BINARY_DIR}/procPidMemWrites.h
	COMMAND ${PROJECT_BINARY_DIR}/proc-pid-mem-write ${CMAKE_CURRENT_BINARY_DIR}/procPidMemWrites.h
	DEPENDS proc-pid-mem-write
)

# we put the header files from the include directory here 
# too so automoc can "just work"
add_library(DebuggerCore SHARED
	${PROJECT_BINARY_DIR}/procPidMemWrites.h
	Breakpoint.cpp
	Breakpoint.h
	DebuggerCoreBase.cpp
	DebuggerCoreBase.h
	unix/DebuggerCoreUNIX.cpp
	unix/DebuggerCoreUNIX.h
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
	unix/linux/PlatformState.cpp
	unix/linux/PlatformState.h
	unix/linux/PlatformThread.cpp
	unix/linux/PlatformThread.h
)

if(Qt5Core_FOUND)
	target_link_libraries(DebuggerCore Qt5::Widgets)
else(Qt5Core_FOUND)
	target_link_libraries(DebuggerCore Qt4::QtGui)
endif()

set(LIBRARY_OUTPUT_PATH    ${PROJECT_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
