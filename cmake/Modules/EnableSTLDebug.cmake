
include("DetectCompiler")

if((TARGET_COMPILER_GCC) OR (TARGET_COMPILER_CLANG))

	option(ENABLE_STL_DEBUG "Enable STL container debugging")

	if(ENABLE_STL_DEBUG)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_DEBUG")
		set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -D_GLIBCXX_DEBUG")	
	endif()
endif()
