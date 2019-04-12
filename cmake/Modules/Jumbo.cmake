

macro(_ENABLE_JUMBO_BUILD VAR EXT SOURCE_LIST)

	set(JUMBO_SOURCE_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/jumbo_source.${EXT}")

	file(WRITE ${JUMBO_SOURCE_FILENAME} "/* Generated Jumbo File */\n")

	foreach(INCLUDE_FILE IN LISTS ${SOURCE_LIST})
		file(APPEND ${JUMBO_SOURCE_FILENAME} "#include \"${INCLUDE_FILE}\"\n")
	endforeach()

	set(${VAR} ${JUMBO_SOURCE_FILENAME})
endmacro()

macro(ENABLE_JUMBO_BUILD TARGET)
	get_target_property(SOURCES ${TARGET} SOURCES)
	
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTOUIC OFF)
	set(CMAKE_AUTORCC OFF)
	
	foreach(FILE IN LISTS SOURCES)
	
		get_filename_component(REALFILE "${FILE}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	
		get_filename_component(EXTENSION ${REALFILE} EXT)
		
		if("${EXTENSION}" STREQUAL ".cpp")
			# handle C++ files
			list(APPEND JUMBO_CPP_SOURCES ${REALFILE})	
			list(REMOVE_ITEM SOURCES ${FILE})
		elseif("${EXTENSION}" STREQUAL ".c")
			# handle C files
			list(APPEND JUMBO_C_SOURCES ${REALFILE})
			list(REMOVE_ITEM SOURCES ${FILE})
		elseif("${EXTENSION}" STREQUAL ".ui")
			# handle Qt UI files
			qt5_wrap_ui(QT_UI_HEADER ${REALFILE})
			list(APPEND QT_UI_HEADERS ${QT_UI_HEADER})
			list(REMOVE_ITEM SOURCES ${FILE})
		elseif("${EXTENSION}" STREQUAL ".qrc")
			# handle Qt QRC files
			qt5_add_resources(QT_QRC_HEADER ${REALFILE})
			list(APPEND QT_QRC_HEADERS ${QT_QRC_HEADER})
			list(REMOVE_ITEM SOURCES ${FILE})
		endif()
	endforeach()
	
	if(JUMBO_CPP_SOURCES)
		_enable_jumbo_build(JUMBO_CPP_SOURCE "cpp" JUMBO_CPP_SOURCES)
		set(SOURCES ${SOURCES} ${JUMBO_CPP_SOURCE})
	endif()
	
	if(JUMBO_C_SOURCES)
		_enable_jumbo_build(JUMBO_C_SOURCE "c" JUMBO_C_SOURCES)
		set(SOURCES ${SOURCES} ${JUMBO_C_SOURCE})
	endif()
	
	set(SOURCES ${SOURCES} ${QT_UI_HEADERS})
	set(SOURCES ${SOURCES} ${QT_QRC_HEADERS})
	
	set_property(TARGET ${TARGET} PROPERTY SOURCES ${SOURCES})
endmacro()
