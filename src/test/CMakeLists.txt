cmake_minimum_required (VERSION 3.1)

add_executable(ValueTest
	Test.cpp
)

target_link_libraries(ValueTest
	edb
)

set_property(TARGET ValueTest PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_property(TARGET ValueTest PROPERTY CXX_STANDARD 14)
set_property(TARGET ValueTest PROPERTY CXX_STANDARD_REQUIRED ON)

add_test(
	NAME ValueTest
	COMMAND $<TARGET_FILE:ValueTest>
)
