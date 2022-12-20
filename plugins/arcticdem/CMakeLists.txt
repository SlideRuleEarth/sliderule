# ArcticDEM SlideRule Plugin top-level CMake build script

project (ARCTICDEM LANGUAGES CXX)

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

####################
# ArcticDem Plugin #
####################

add_library (arcticdem MODULE "")

set_target_properties (arcticdem PROPERTIES OUTPUT_NAME arcticdem)
set_target_properties (arcticdem PROPERTIES PREFIX "")

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Version Information #
file(STRINGS ${PROJECT_SOURCE_DIR}/../../version.txt TGTVER)
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (arcticdem PUBLIC BUILDINFO="${BUILDINFO}")

# Set Environment Variables #
set (INSTALLDIR /usr/local CACHE STRING "Installation directory for library and executables")
set (CONFDIR ${INSTALLDIR}/etc/sliderule)

# Compile Definitions #
target_compile_definitions (arcticdem PUBLIC BINID="${TGTVER}")
if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (arcticdem PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Link Options #
if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (arcticdem PUBLIC -fsanitize=address)
endif()

# Source Files #
target_sources(arcticdem
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/plugin/arcticdem.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/ArcticDemMosaicRaster.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/ArcticDemStripsRaster.cpp
)

# Include Directories #
target_include_directories (arcticdem
    PUBLIC
        ${INSTALLDIR}/include/sliderule
        ${LUA_INCLUDE_DIR}
        ${GDAL_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# Plugin Installation #
install (TARGETS arcticdem LIBRARY DESTINATION ${CONFDIR})

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/plugin/arcticdem.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/ArcticDemMosaicRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/ArcticDemStripsRaster.h
    DESTINATION
        ${INSTALLDIR}/include/sliderule)
