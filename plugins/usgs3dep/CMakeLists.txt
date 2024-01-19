# usgs3dep SlideRule Plugin top-level CMake build script

project (USGS3DEP LANGUAGES CXX)
cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
include(${CMAKE_SOURCE_DIR}/../../project-config.cmake)

####################
# usgs3dep Plugin   #
####################

add_library (usgs3dep MODULE "")

set_target_properties (usgs3dep PROPERTIES OUTPUT_NAME usgs3dep)
set_target_properties (usgs3dep PROPERTIES PREFIX "")
set_target_properties (usgs3dep PROPERTIES CXX_STANDARD ${CXX_VERSION})

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Build Information #
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (usgs3dep PUBLIC BUILDINFO="${BUILDINFO}")

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
