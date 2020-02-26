
include("DetectCompiler")

function(JOIN VALUES GLUE OUTPUT)
  string (REPLACE ";" "${GLUE}" _TMP_STR "${VALUES}")
  set (${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()

if((TARGET_COMPILER_GCC) OR (TARGET_COMPILER_CLANG))
	option(ENABLE_ASAN        "Enable address sanitizer")
	option(ENABLE_MSAN        "Enable memory sanitizer")
	option(ENABLE_USAN        "Enable undefined sanitizer")
	option(ENABLE_TSAN        "Enable thread sanitizer")

	set(SANITIZERS "")

	if(ENABLE_ASAN)
		list(APPEND SANITIZERS "address")
	endif()

	if(ENABLE_MSAN)
		list(APPEND SANITIZERS "memory")
	endif()

	if(ENABLE_USAN)
		list(APPEND SANITIZERS "undefined")
	endif()

	if(ENABLE_TSAN)
		list(APPEND SANITIZERS "thread")
	endif()

	JOIN("${SANITIZERS}" "," LIST_OF_SANITIZERS)
endif()

if(LIST_OF_SANITIZERS)
	if(NOT "${LIST_OF_SANITIZERS}" STREQUAL "")
		set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}        -fsanitize=${LIST_OF_SANITIZERS}")
		set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS}          -fsanitize=${LIST_OF_SANITIZERS}")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${LIST_OF_SANITIZERS}")
	endif()
endif()

