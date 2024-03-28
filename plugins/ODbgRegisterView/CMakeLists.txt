cmake_minimum_required (VERSION 3.15)
include("GNUInstallDirs")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(PLUGIN_NAME "ODbgRegisterView")

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

add_library(${PLUGIN_NAME} SHARED ${PluginSources})

target_include_directories(${PLUGIN_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${PLUGIN_INCLUDES}
)

target_link_libraries(${PLUGIN_NAME} Qt5::Widgets edb)

install (TARGETS ${PLUGIN_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb)

target_add_warnings(${PLUGIN_NAME})

set_target_properties(${PLUGIN_NAME}
    PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
	LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)
