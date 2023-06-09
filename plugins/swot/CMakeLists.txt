# SWOT SlideRule Plugin top-level CMake build script

project (SWOT LANGUAGES CXX)

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
# SWOT Plugin #
###############

add_library (swot MODULE "")

set_target_properties (swot PROPERTIES OUTPUT_NAME swot)
set_target_properties (swot PROPERTIES PREFIX "")

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Version Information #
file(STRINGS ${PROJECT_SOURCE_DIR}/../../version.txt TGTVER)
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (swot PUBLIC BUILDINFO="${BUILDINFO}")

# Set Environment Variables #
set (INSTALLDIR /usr/local CACHE STRING "Installation directory for library and executables")
set (CONFDIR ${INSTALLDIR}/etc/sliderule)

# Compile Definitions #
target_compile_definitions (swot PUBLIC BINID="${TGTVER}")
if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (swot PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Address Sanitizer Option #
if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (swot PUBLIC -fsanitize=address)
endif()

# Source Files #
target_sources(swot
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/plugin/swot.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/SwotL2Reader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/plugin/SwotParms.cpp
)

# Include Directories #
target_include_directories (swot
    PUBLIC
        ${INSTALLDIR}/include/sliderule
        ${LUA_INCLUDE_DIR}
        ${GDAL_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# Script Installation #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/swotl2.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/swotl2p.lua
    DESTINATION
        ${CONFDIR}/api
)

# Plugin Installation #
install (TARGETS swot LIBRARY DESTINATION ${CONFDIR})

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/plugin/swot.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/SwotL2Reader.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/SwotParms.h
    DESTINATION
        ${INSTALLDIR}/include/sliderule/swot
)
