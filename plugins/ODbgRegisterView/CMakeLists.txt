cmake_minimum_required (VERSION 3.0)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(PluginName "ODbgRegisterView")

find_package(Qt5 5.0.0 REQUIRED Widgets)

set(PluginSources
	BitFieldDescription.h
	BitFieldDescription.cpp
	FPUValueField.h
	SIMDValueManager.cpp
	SIMDValueManager.h
	RegisterGroup.cpp
	RegisterGroup.h
	MultiBitFieldWidget.cpp
	MultiBitFieldWidget.h
	BitFieldFormatter.cpp
	BitFieldFormatter.h
	VolatileNameField.cpp
	VolatileNameField.h
	Canvas.cpp
	Canvas.h
	DialogEditGPR.cpp
	DialogEditGPR.h
	DialogEditSIMDRegister.cpp
	DialogEditSIMDRegister.h
	EntryGridKeyUpDownEventFilter.cpp
	EntryGridKeyUpDownEventFilter.h
	FieldWidget.cpp
	FieldWidget.h
	GPREdit.cpp
	GPREdit.h
	NumberEdit.cpp
	NumberEdit.h
	ODbgRV_Common.h
	ODbgRV_Util.h
	ODbgRV_x86Common.h
	Plugin.cpp
	Plugin.h
	RegisterView.cpp
	RegisterView.h
	ValueField.cpp
	ValueField.h
)

if(TARGET_ARCH_FAMILY_X86)
	set(PluginSources
		${PluginSources}
		x86Groups.cpp
		x86Groups.h
		x86FPUValueField.cpp
		Float80Edit.cpp
		Float80Edit.h
		DialogEditFPU.cpp
		DialogEditFPU.h
	)
elseif(TARGET_ARCH_FAMILY_ARM)
	set(PluginSources
		${PluginSources}
		armGroups.cpp
		armGroups.h
	)
endif()

add_library(${PluginName} SHARED ${PluginSources})

target_link_libraries(${PluginName} Qt5::Widgets edb)

install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

set_property(TARGET ${PluginName} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${PluginName} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_property(TARGET ${PluginName} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

if (ENABLE_JUMBO_BUILD)
	enable_jumbo_build(${PluginName})
endif()
