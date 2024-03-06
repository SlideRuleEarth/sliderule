# GEDI SlideRule Plugin top-level CMake build script

project (GEDI LANGUAGES CXX)
cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
include(${CMAKE_SOURCE_DIR}/../../project-config.cmake)

###############
# GEDI Plugin #
###############

add_library (gedi MODULE "")

set_target_properties (gedi PROPERTIES OUTPUT_NAME gedi)
set_target_properties (gedi PROPERTIES PREFIX "")
set_target_properties (gedi PROPERTIES CXX_STANDARD ${CXX_VERSION})

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Build Information #
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (gedi PUBLIC BUILDINFO="${BUILDINFO}")

# Compile Definitions #
target_compile_definitions (gedi PUBLIC BINID="${TGTVER}")
target_compile_options (gedi PUBLIC -Wall) # turn on "all" warnings
target_compile_options (gedi PUBLIC -Wextra) # turn on "extra" warnings
target_compile_options (gedi PUBLIC -Wreorder) # turn on warning for object initializer list order enforcement
target_compile_options (gedi PUBLIC -Wshadow) # turn on warning for inner scope var with same name as outer scope var
if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (gedi PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Address Sanitizer Option #
if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (gedi PUBLIC -fsanitize=address)
endif()

# Source Files #
target_sources(gedi
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/plugin/gedi.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi01bReader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi02aReader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi04aReader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/GediIODriver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/GediParms.cpp
)

# Include Directories #
target_include_directories (gedi
    PUBLIC
        ${INSTALLDIR}/include/sliderule
        ${LUA_INCLUDE_DIR}
        ${GDAL_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# Script Installation #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi01b.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi01bp.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi02a.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi02ap.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi04a.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi04ap.lua
    DESTINATION
        ${CONFDIR}/api
)

# Plugin Installation #
install (TARGETS gedi LIBRARY DESTINATION ${CONFDIR})

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/plugin/gedi.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/FootprintReader.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi01bReader.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi02aReader.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi03Raster.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi04aReader.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi04bRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/GediIODriver.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/GediParms.h
    DESTINATION
        ${INSTALLDIR}/include/sliderule/gedi
)
