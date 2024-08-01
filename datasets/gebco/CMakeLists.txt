# gebco SlideRule Plugin top-level CMake build script

cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
project (GEBCO LANGUAGES CXX)
include(${CMAKE_SOURCE_DIR}/../../project-config.cmake)

####################
# gebco Plugin   #
####################

add_library (gebco MODULE "")

set_target_properties (gebco PROPERTIES OUTPUT_NAME gebco)
set_target_properties (gebco PROPERTIES PREFIX "")
set_target_properties (gebco PROPERTIES CXX_STANDARD ${CXX_VERSION})

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Build Information #
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (gebco PUBLIC BUILDINFO="${BUILDINFO}")

# Compile Definitions #
target_compile_definitions (gebco PUBLIC BINID="${TGTVER}")
target_compile_options (gebco PUBLIC -Wall) # turn on "all" warnings
target_compile_options (gebco PUBLIC -Wextra) # turn on "extra" warnings
target_compile_options (gebco PUBLIC -Wreorder) # turn on warning for object initializer list order enforcement
target_compile_options (gebco PUBLIC -Wshadow) # turn on warning for inner scope var with same name as outer scope var
if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (gebco PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Link Options #
if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (gebco PUBLIC -fsanitize=address)
endif()

# Source Files #
target_sources(gebco
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/plugin/gebco.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/GebcoBathyRaster.cpp
)

# Include Directories #
target_include_directories (gebco
    PUBLIC
        ${INSTALLDIR}/include/sliderule
        ${LUA_INCLUDE_DIR}
        ${GDAL_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# Plugin Installation #
install (TARGETS gebco LIBRARY DESTINATION ${CONFDIR})

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/plugin/gebco.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/GebcoBathyRaster.h
    DESTINATION
        ${INSTALLDIR}/include/sliderule/gebco
)
