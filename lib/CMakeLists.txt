cmake_minimum_required(VERSION 3.15)
add_subdirectory(libELF)
add_subdirectory(libPE)

if(UNIX)
	if(TARGET_ARCH_FAMILY_X86 AND NOT GDTOA_FOUND)
		add_subdirectory(gdtoa-desktop)
	endif()
endif()


