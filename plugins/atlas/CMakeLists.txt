# ATLAS SlideRule Plugin top-level CMake build script

project (ATLAS LANGUAGES CXX)
cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
include(${CMAKE_SOURCE_DIR}/../../project-config.cmake)

################
# ATLAS Plugin #
################

add_library (atlas MODULE "")

set_target_properties (atlas PROPERTIES OUTPUT_NAME atlas)
set_target_properties (atlas PROPERTIES PREFIX "")

# Prerequisites #
find_package (Lua "5.3")

# Compile Definitions #
target_compile_definitions (atlas PUBLIC BINID="${TGTVER}")
target_compile_definitions (atlas PUBLIC __gnu__)

# Source Files #
target_sources(atlas
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/plugin/atlas.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/ItosRecord.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/ItosRecordParser.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/AtlasFileWriter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/AtlasHistogram.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/AltimetryHistogram.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/TimeTagHistogram.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/TimeTagProcessorModule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/AltimetryProcessorModule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/MajorFrameProcessorModule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/TimeProcessorModule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/LaserProcessorModule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/CmdEchoProcessorModule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/DiagLogProcessorModule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/HstvsSimulator.cpp
)

# Include Directories #
target_include_directories (atlas
    PUBLIC
        ${INSTALLDIR}/include/sliderule
        ${LUA_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# Script Installation #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl00.lua
    DESTINATION
        ${CONFDIR}/api
)

# Plugin Installation #
install (TARGETS atlas LIBRARY DESTINATION ${CONFDIR})
