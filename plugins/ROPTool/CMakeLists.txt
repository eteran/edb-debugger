cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "ROPTool")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED Widgets)

add_library(${PLUGIN_NAME} SHARED
    DialogROPTool.cpp
    DialogROPTool.h
    DialogROPTool.ui
    ROPTool.cpp
    ROPTool.h
    DialogResults.ui
    DialogResults.cpp
    DialogResults.h
    ResultsModel.cpp
    ResultsModel.h
)

target_link_libraries(${PLUGIN_NAME} Qt${QT_VERSION_MAJOR}::Widgets edb)

build_plugin(${PLUGIN_NAME})
