# GEDI SlideRule Plugin top-level CMake build script

project (GEDI LANGUAGES CXX)

#################
# CMake Options #
#################

set (CMAKE_LEGACY_CYGWIN_WIN32 0) # Squelch a warning when building on Win32/Cygwin
cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
cmake_policy(SET CMP0079 NEW) # add link library to target which is not built in this directory
cmake_policy(SET CMP0053 NEW) # simplified variable escape processing (recommended by cmake)

###################
# Project Options #
###################

option (ENABLE_ADDRESS_SANITIZER "Instrument code with AddressSanitizer for memory error detection" OFF)

###############
# GEDI Plugin #
###############

add_library (gedi MODULE "")

set_target_properties (gedi PROPERTIES OUTPUT_NAME gedi)
set_target_properties (gedi PROPERTIES PREFIX "")

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Version Information #
file(STRINGS ${PROJECT_SOURCE_DIR}/../../version.txt TGTVER)
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (gedi PUBLIC BUILDINFO="${BUILDINFO}")

# Set Environment Variables #
set (INSTALLDIR /usr/local CACHE STRING "Installation directory for library and executables")
set (CONFDIR ${INSTALLDIR}/etc/sliderule)

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
    ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi03Raster.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi04aReader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/Gedi04bRaster.cpp
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
        ${CMAKE_CURRENT_LIST_DIR}/plugin/GediParms.h
    DESTINATION
        ${INSTALLDIR}/include/sliderule/gedi
)
