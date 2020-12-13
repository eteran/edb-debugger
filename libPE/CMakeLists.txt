cmake_minimum_required(VERSION 3.1)

add_library(PE INTERFACE
)

target_include_directories(PE
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(PE INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/include/libPE/pe_binary.h

)

