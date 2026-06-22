cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "ProcessProperties")

find_package(Qt5 REQUIRED Widgets Network)

add_library(${PLUGIN_NAME} SHARED
    DialogProcessProperties.cpp
    DialogProcessProperties.h
    DialogProcessProperties.ui
    DialogStrings.cpp
    DialogStrings.h
    DialogStrings.ui
    ProcessProperties.cpp
    ProcessProperties.h
    DialogResults.cpp
    DialogResults.h
    DialogResults.ui
    ResultsModel.cpp
    ResultsModel.h
)

target_link_libraries(${PLUGIN_NAME} Qt5::Widgets Qt5::Network edb)

build_plugin(${PLUGIN_NAME})
