# PGC SlideRule Plugin top-level CMake build script

cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
project (PGC LANGUAGES CXX)
include(${CMAKE_SOURCE_DIR}/../../project-config.cmake)

##############
# PGC Plugin #
##############

add_library (pgc MODULE "")

set_target_properties (pgc PROPERTIES OUTPUT_NAME pgc)
set_target_properties (pgc PROPERTIES PREFIX "")
set_target_properties (pgc PROPERTIES CXX_STANDARD ${CXX_VERSION})

# Prerequisites #
find_package (Lua "5.3")
find_package (GDAL)

# Version Information #
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (pgc PUBLIC BUILDINFO="${BUILDINFO}")

# Compile Definitions #
target_compile_definitions (pgc PUBLIC BINID="${TGTVER}")
target_compile_options (pgc PUBLIC -Wall) # turn on "all" warnings
target_compile_options (pgc PUBLIC -Wextra) # turn on "extra" warnings
target_compile_options (pgc PUBLIC -Wreorder) # turn on warning for object initializer list order enforcement
target_compile_options (pgc PUBLIC -Wshadow) # turn on warning for inner scope var with same name as outer scope var
if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (pgc PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Link Options #
if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (pgc PUBLIC -fsanitize=address)
endif()

# Source Files #
target_sources(pgc
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/plugin/pgc.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/PgcWkt.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/PgcDemStripsRaster.cpp
)

# Include Directories #
target_include_directories (pgc
    PUBLIC
        ${INSTALLDIR}/include/sliderule
        ${LUA_INCLUDE_DIR}
        ${GDAL_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# Plugin Installation #
install (TARGETS pgc LIBRARY DESTINATION ${CONFDIR})

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/plugin/pgc.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/PgcWkt.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/PgcDemStripsRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/ArcticDemMosaicRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/RemaDemMosaicRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/ArcticDemStripsRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/RemaDemStripsRaster.h
    DESTINATION
        ${INSTALLDIR}/include/sliderule/pgc
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/plugin/ITRF2014_3031.wkt
        ${CMAKE_CURRENT_LIST_DIR}/plugin/ITRF2014_3413.wkt
    DESTINATION
        ${CONFDIR}
)
