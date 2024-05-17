cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
project (CLOUD LANGUAGES CXX)
include(${CMAKE_SOURCE_DIR}/../../project-config.cmake)

message (STATUS "Configuring cloud deployment")
install (FILES ${CMAKE_CURRENT_LIST_DIR}/asset_directory.csv DESTINATION ${CONFDIR})
