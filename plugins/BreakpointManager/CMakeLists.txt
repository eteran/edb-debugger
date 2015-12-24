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
)

# we put the header files from the include directory here 
# too so automoc can "just work"
add_library(BreakpointManager SHARED
	BreakpointManager.cpp
	BreakpointManager.h
	DialogBreakpoints.cpp
	DialogBreakpoints.h
)

if(Qt5Core_FOUND)
	target_link_libraries(BreakpointManager Qt5::Widgets)
else(Qt5Core_FOUND)
	target_link_libraries(BreakpointManager Qt4::QtGui)
endif()

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
