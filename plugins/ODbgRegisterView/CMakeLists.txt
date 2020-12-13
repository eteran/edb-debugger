cmake_minimum_required (VERSION 3.1)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(PluginName "ODbgRegisterView")

find_package(Qt5 5.0.0 REQUIRED Widgets)

set(PluginSources
	BitFieldDescription.cpp
	BitFieldDescription.h
	BitFieldFormatter.cpp
	BitFieldFormatter.h
	Canvas.cpp
	Canvas.h
	DialogEditGPR.cpp
	DialogEditGPR.h
	DialogEditSimdRegister.cpp
	DialogEditSimdRegister.h
	EntryGridKeyUpDownEventFilter.cpp
	EntryGridKeyUpDownEventFilter.h
	FieldWidget.cpp
	FieldWidget.h
	FpuValueField.h
	GprEdit.cpp
	GprEdit.h
	MultiBitFieldWidget.cpp
	MultiBitFieldWidget.h
	NumberEdit.cpp
	NumberEdit.h
	ODbgRV_Common.h
	ODbgRV_Util.h	
	Plugin.cpp
	Plugin.h
	RegisterGroup.cpp
	RegisterGroup.h
	RegisterView.cpp
	RegisterView.h
	SimdValueManager.cpp
	SimdValueManager.h
	ValueField.cpp
	ValueField.h
	VolatileNameField.cpp
	VolatileNameField.h
)

if(TARGET_ARCH_FAMILY_X86)

	set(PLUGIN_INCLUDES
		arch/x86-generic
	)
	
	set(PluginSources
		${PluginSources}
		arch/x86-generic/x86Groups.cpp
		arch/x86-generic/x86Groups.h
		arch/x86-generic/x86FPUValueField.cpp
		arch/x86-generic/Float80Edit.cpp
		arch/x86-generic/Float80Edit.h
		arch/x86-generic/DialogEditFPU.cpp
		arch/x86-generic/DialogEditFPU.h
		arch/x86-generic/ODbgRV_x86Common.h
	)
elseif(TARGET_ARCH_FAMILY_ARM)

	set(PLUGIN_INCLUDES
		arch/arm-generic
	)

	set(PluginSources
		${PluginSources}
		arch/arm-generic/armGroups.cpp
		arch/arm-generic/armGroups.h
	)
endif()

add_library(${PluginName} SHARED ${PluginSources})

target_include_directories(${PluginName} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${PLUGIN_INCLUDES}
)

target_link_libraries(${PluginName} Qt5::Widgets edb)

install (TARGETS ${PluginName} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

set_property(TARGET ${PluginName} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD 14)
set_property(TARGET ${PluginName} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PluginName} PROPERTY LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_property(TARGET ${PluginName} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
