# ICESat-2 SlideRule Plugin top-level CMake build script

cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
project (ICESAT2 LANGUAGES CXX)
include(${CMAKE_SOURCE_DIR}/../../project-config.cmake)

###################
# ICESat-2 Plugin #
###################

add_library (icesat2 MODULE "")

set_target_properties (icesat2 PROPERTIES OUTPUT_NAME icesat2)
set_target_properties (icesat2 PROPERTIES PREFIX "")
set_target_properties (icesat2 PROPERTIES CXX_STANDARD ${CXX_VERSION})

# Prerequisites #
find_package (Lua "5.3")

# Build Information #
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../.. --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (icesat2 PUBLIC BUILDINFO="${BUILDINFO}")

# Compile Definitions #
target_compile_definitions (icesat2 PUBLIC BINID="${TGTVER}")
target_compile_options (icesat2 PUBLIC -Wall) # turn on "all" warnings
target_compile_options (icesat2 PUBLIC -Wextra) # turn on "extra" warnings
target_compile_options (icesat2 PUBLIC -Wreorder) # turn on warning for object initializer list order enforcement
target_compile_options (icesat2 PUBLIC -Wshadow) # turn on warning for inner scope var with same name as outer scope var
if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (icesat2 PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Address Sanitizer Option #
if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (icesat2 PUBLIC -fsanitize=address)
endif()

# Source Files #
target_sources(icesat2
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/plugin/icesat2.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl03Reader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl03Indexer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl03BathyReader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl06Dispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl06Reader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl08Dispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl13IODriver.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl13Reader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/BathyParms.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/CumulusIODriver.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/GTDArray.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Icesat2Parms.cpp
        ${CMAKE_CURRENT_LIST_DIR}/plugin/MeritRaster.cpp
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/plugin/UT_Atl03Reader.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/plugin/UT_Atl06Dispatch.cpp>)

# Include Directories #
target_include_directories (icesat2
    PUBLIC
        ${INSTALLDIR}/include/sliderule
        ${LUA_INCLUDE_DIR}
        ${GDAL_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/plugin
)

# API Script Installation #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl03s.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl03sp.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl06.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl06p.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl06s.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl06sp.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl08.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl08p.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl13s.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl13sp.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl24g.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl24gp.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/indexer.lua
    DESTINATION
        ${CONFDIR}/api
)

# API Script Installation #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/extensions/atl24_init.lua
    DESTINATION
        ${CONFDIR}
)

# Plugin Installation #
install (TARGETS icesat2 LIBRARY DESTINATION ${CONFDIR})

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/plugin/icesat2.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl03Reader.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl03Indexer.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl03BathyReader.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl06Dispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl06Reader.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl08Dispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl13IODriver.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Atl13Reader.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/BathyParms.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/CumulusIODriver.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/GTArray.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/GTDArray.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/Icesat2Parms.h
        ${CMAKE_CURRENT_LIST_DIR}/plugin/MeritRaster.h
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/plugin/UT_Atl03Reader.h>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/plugin/UT_Atl06Dispatch.h>
    DESTINATION
        ${INSTALLDIR}/include/sliderule/icesat2
)
