cmake_minimum_required (VERSION 3.0)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(PluginName "ODbgRegisterView")

find_package(Qt5 5.0.0 REQUIRED Widgets)

set(PluginSources
	DialogEditGPR.cpp
	GPREdit.cpp
	Plugin.cpp
	Plugin.h
	RegisterView.cpp
	RegisterView.h
	ValueField.cpp
	FieldWidget.cpp
	DialogEditSIMDRegister.cpp
	NumberEdit.cpp
	EntryGridKeyUpDownEventFilter.cpp
)
if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "i[3456]86") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64"))
	set(PluginSources
		${PluginSources}
		x86Groups.cpp
		x86FPUValueField.cpp
		Float80Edit.cpp
		DialogEditFPU.cpp
	)
endif()
if((${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv[0-9]+"))
	set(PluginSources
		${PluginSources}
		armGroups.cpp
	)
endif()

# we put the header files from the include directory here
# too so automoc can "just work"
add_library(${PluginName} SHARED ${PluginSources})

target_link_libraries(${PluginName} Qt5::Widgets)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

set_property(TARGET ${PluginName} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD 14)
