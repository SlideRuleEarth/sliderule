# OPENDATA SlideRule Plugin top-level CMake build script

project (OPENDATA LANGUAGES CXX)

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
# OPENDATA Plugin   #
####################

add_library (opendata MODULE "")

set_target_properties (opendata PROPERTIES OUTPUT_NAME opendata)
set_target_properties (opendata PROPERTIES PREFIX "")

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Version Information #
file(STRINGS ${PROJECT_SOURCE_DIR}/../../version.txt TGTVER)
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (opendata PUBLIC BUILDINFO="${BUILDINFO}")

# Set Environment Variables #
set (INSTALLDIR /usr/local CACHE STRING "Installation directory for library and executables")
set (CONFDIR ${INSTALLDIR}/etc/sliderule)

# Compile Definitions #
target_compile_definitions (opendata PUBLIC BINID="${TGTVER}")
target_compile_options (opendata PUBLIC -Wall) # turn on "all" warnings
target_compile_options (opendata PUBLIC -Wextra) # turn on "extra" warnings
target_compile_options (opendata PUBLIC -Wreorder) # turn on warning for object initializer list order enforcement
target_compile_options (opendata PUBLIC -Wshadow) # turn on warning for inner scope var with same name as outer scope var
if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (opendata PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Link Options #
if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (opendata PUBLIC -fsanitize=address)
endif()

# Source Files #
target_sources(opendata
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/plugin/opendata.cpp
)

# Include Directories #
target_include_directories (opendata
    PUBLIC
        ${INSTALLDIR}/include/sliderule
        ${LUA_INCLUDE_DIR}
        ${GDAL_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# Plugin Installation #
install (TARGETS opendata LIBRARY DESTINATION ${CONFDIR})

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/plugin/opendata.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/EsaWorldCover10meterRaster.h
    DESTINATION
        ${INSTALLDIR}/include/sliderule/opendata
)
