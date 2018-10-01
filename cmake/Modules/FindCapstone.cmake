if (UNIX)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(CAPSTONE REQUIRED capstone>=3.0.4)
elseif (WIN32)
	# CAPSTONE_SDK should be the path to a directory containing a subdirectory "include/capstone"
	# with all the include headers of capstone and another subdirectory "lib" with "capstone_dll.lib" or "libcapstone.dll".
	if (NOT DEFINED CAPSTONE_SDK)
		set(CAPSTONE_SDK NOTFOUND CACHE PATH "Path to the Capstone SDK")
		message(SEND_ERROR "Path to Capstone SDK is missing. Please specify CAPSTONE_SDK.")
	endif()

	find_path(CAPSTONE_INCLUDE_DIRS
		capstone/capstone.h
		PATHS ${CAPSTONE_SDK}/include
	)

    if (NOT CAPSTONE_INCLUDE_DIRS)
		message(SEND_ERROR "Include directory for Capstone not found. Please specify CAPSTONE_SDK.")
	endif()

	find_library(CAPSTONE_LIBRARIES
		NAMES capstone_dll libcapstone
		HINTS "${CAPSTONE_SDK}/lib"
	)

    if (NOT CAPSTONE_LIBRARIES)
		message(SEND_ERROR "Library directory for Capstone not found. Please specify CAPSTONE_SDK.")
	endif()
endif()
