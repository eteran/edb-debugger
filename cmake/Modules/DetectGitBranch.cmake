
function(git_get_branch RESULT)

	if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
		execute_process(
			COMMAND git rev-parse HEAD
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			OUTPUT_VARIABLE OUTPUT
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		set(${RESULT} ${OUTPUT} PARENT_SCOPE)
	else()
		set(${RESULT} "Unknown" PARENT_SCOPE)
	endif()

endfunction()
