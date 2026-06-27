cmake_minimum_required (VERSION 3.15)
include("BuildPlugin")

set(PLUGIN_NAME "FasLoader")

find_package(Qt${QT_VERSION_MAJOR} REQUIRED Widgets)

add_library(${PLUGIN_NAME} SHARED
  FasLoader.cpp
  FasLoader.hpp

  Fas/Core.cpp
  Fas/Core.hpp
  Fas/Exception.cpp
  Fas/Exception.hpp
  Fas/Header.hpp
  Fas/PluginSymbol.hpp
  Fas/Symbol.hpp
)

target_link_libraries(${PLUGIN_NAME} Qt::Widgets edb)

build_plugin(${PLUGIN_NAME})
