
include("DetectCompiler")

function(TARGET_ADD_WARNINGS TARGET)
	if((TARGET_COMPILER_GCC) OR (TARGET_COMPILER_CLANG))
		target_compile_options(${TARGET}
		PUBLIC
			-W
			-Wall
			#-Wshadow
			-Wnon-virtual-dtor
			#-Wold-style-cast
			-Wcast-align
			-Wunused
			-Woverloaded-virtual
			-pedantic
			#-Wconversion
			#-Wsign-conversion
			#-Wnull-dereference
			-Wdouble-promotion
			-Wformat=2
			-Wno-unused-macros
			-Wno-switch-enum
			-Wno-unknown-pragmas
		)

		if(TARGET_COMPILER_CLANG)
			target_compile_options(${TARGET}
			PUBLIC
				#-Wexit-time-destructors
				#-Wglobal-constructors
				#-Wshadow-uncaptured-local
				#-Wshorten-64-to-32
				-Wconditional-uninitialized
				-Wimplicit-fallthrough
				-Wmissing-prototypes
			)

	    elseif(TARGET_COMPILER_GCC)
			target_compile_options(${TARGET}
			PUBLIC
				#-Wduplicated-branches
				#-Wduplicated-cond
				#-Wsuggest-attribute=const
				#-Wsuggest-attribute=noreturn
				#-Wsuggest-attribute=pure
				#-Wsuggest-final-methods
				#-Wsuggest-final-types
				#-Wuseless-cast
				-Wlogical-op
				-Wsuggest-override

			)
	    endif()
	endif()
endfunction()
