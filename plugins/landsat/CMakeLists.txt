# Landsat SlideRule Plugin top-level CMake build script

project (LANDSAT LANGUAGES CXX)
cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
include(${CMAKE_SOURCE_DIR}/../../project-config.cmake)

####################
# Landsat Plugin   #
####################

add_library (landsat MODULE "")

set_target_properties (landsat PROPERTIES OUTPUT_NAME landsat)
set_target_properties (landsat PROPERTIES PREFIX "")
set_target_properties (landsat PROPERTIES CXX_STANDARD ${CXX_VERSION})

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Build Information #
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (landsat PUBLIC BUILDINFO="${BUILDINFO}")

# Compile Definitions #
target_compile_definitions (landsat PUBLIC BINID="${TGTVER}")
target_compile_options (landsat PUBLIC -Wall) # turn on "all" warnings
target_compile_options (landsat PUBLIC -Wextra) # turn on "extra" warnings
target_compile_options (landsat PUBLIC -Wreorder) # turn on warning for object initializer list order enforcement
target_compile_options (landsat PUBLIC -Wshadow) # turn on warning for inner scope var with same name as outer scope var
if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (landsat PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Link Options #
if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (landsat PUBLIC -fsanitize=address)
endif()

# Source Files #
target_sources(landsat
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/plugin/landsat.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/LandsatHlsRaster.cpp
)

# Include Directories #
target_include_directories (landsat
    PUBLIC
        ${INSTALLDIR}/include/sliderule
        ${LUA_INCLUDE_DIR}
        ${GDAL_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# Plugin Installation #
install (TARGETS landsat LIBRARY DESTINATION ${CONFDIR})

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/plugin/landsat.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/LandsatHlsRaster.h
    DESTINATION
        ${INSTALLDIR}/include/sliderule/landsat
)
