# SWOT SlideRule Plugin top-level CMake build script

project (SWOT LANGUAGES CXX)
cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
include(${CMAKE_SOURCE_DIR}/../../project-config.cmake)

###############
# SWOT Plugin #
###############

add_library (swot MODULE "")

set_target_properties (swot PROPERTIES OUTPUT_NAME swot)
set_target_properties (swot PROPERTIES PREFIX "")
set_target_properties (swot PROPERTIES CXX_STANDARD ${CXX_VERSION})

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Build Information #
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (swot PUBLIC BUILDINFO="${BUILDINFO}")

# Compile Definitions #
target_compile_definitions (swot PUBLIC BINID="${TGTVER}")
target_compile_options (swot PUBLIC -Wall) # turn on "all" warnings
target_compile_options (swot PUBLIC -Wextra) # turn on "extra" warnings
target_compile_options (swot PUBLIC -Wreorder) # turn on warning for object initializer list order enforcement
target_compile_options (swot PUBLIC -Wshadow) # turn on warning for inner scope var with same name as outer scope var
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
