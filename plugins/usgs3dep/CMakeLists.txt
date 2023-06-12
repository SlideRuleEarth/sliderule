# usgs3dep SlideRule Plugin top-level CMake build script

project (USGS3DEP LANGUAGES CXX)

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
# usgs3dep Plugin   #
####################

add_library (usgs3dep MODULE "")

set_target_properties (usgs3dep PROPERTIES OUTPUT_NAME usgs3dep)
set_target_properties (usgs3dep PROPERTIES PREFIX "")

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Version Information #
file(STRINGS ${PROJECT_SOURCE_DIR}/../../version.txt TGTVER)
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (usgs3dep PUBLIC BUILDINFO="${BUILDINFO}")

# Set Environment Variables #
set (INSTALLDIR /usr/local CACHE STRING "Installation directory for library and executables")
set (CONFDIR ${INSTALLDIR}/etc/sliderule)

# Compile Definitions #
target_compile_definitions (usgs3dep PUBLIC BINID="${TGTVER}")
target_compile_options (usgs3dep PUBLIC -Wall) # turn on "all" warnings
target_compile_options (usgs3dep PUBLIC -Wextra) # turn on "extra" warnings
target_compile_options (usgs3dep PUBLIC -Wreorder) # turn on warning for object initializer list order enforcement
target_compile_options (usgs3dep PUBLIC -Wshadow) # turn on warning for inner scope var with same name as outer scope var
if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (usgs3dep PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Link Options #
if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (usgs3dep PUBLIC -fsanitize=address)
endif()

# Source Files #
target_sources(usgs3dep
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/plugin/usgs3dep.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Usgs3dep1meterDemRaster.cpp
)

# Include Directories #
target_include_directories (usgs3dep
    PUBLIC
        ${INSTALLDIR}/include/sliderule
        ${LUA_INCLUDE_DIR}
        ${GDAL_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# Plugin Installation #
install (TARGETS usgs3dep LIBRARY DESTINATION ${CONFDIR})

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/plugin/usgs3dep.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Usgs3dep1meterDemRaster.h
    DESTINATION
        ${INSTALLDIR}/include/sliderule/usgs3dep
)
