# ATL24 SlideRule Plugin top-level CMake build script
cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
project (ATL24 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")
set(CMAKE_C_FLAGS_DEBUG "-Og -g")

# Squelch a warning when building on Win32/Cygwin
set (CMAKE_LEGACY_CYGWIN_WIN32 0)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(default_build_type "Debug")
endif()

# ATL24 Plugin #
add_library (atl24 MODULE "")
set_target_properties (atl24 PROPERTIES OUTPUT_NAME atl24)
set_target_properties (atl24 PROPERTIES PREFIX "")

# Dependencies #
find_package (Lua "5.3")
find_package(xgboost REQUIRED)
find_package(LibArchive REQUIRED)
find_library(LIBUUID_LIBRARY libuuid.so)
target_link_libraries (atl24 PUBLIC ${LIBUUID_LIBRARY})
target_link_libraries(atl24 PUBLIC xgboost::xgboost)
target_link_libraries(atl24 PUBLIC LibArchive::LibArchive)

# Version Information #
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR} --git-dir ${PROJECT_SOURCE_DIR}/../../.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (atl24 PUBLIC BUILDINFO="${BUILDINFO}")
file(STRINGS ${PROJECT_SOURCE_DIR}/../../version.txt TGTVER)
target_compile_definitions (atl24 PUBLIC BINID="${TGTVER}")

# Algorithm Version Information #
execute_process (COMMAND git --work-tree ${PROJECT_SOURCE_DIR}/../../../atl24_v2_algorithms --git-dir ${PROJECT_SOURCE_DIR}/../../../atl24_v2_algorithms/.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE ALGOINFO)
string(REGEX REPLACE "\n$" "" ALGOINFO "${ALGOINFO}")
target_compile_definitions (atl24 PUBLIC ALGOINFO="${ALGOINFO}")

# Set Environment Variables #
set (INSTALLDIR /usr/local CACHE STRING "Installation directory for library and executables")
set (CONFDIR ${INSTALLDIR}/etc/sliderule)
set (ATL24DIR ${INSTALLDIR} CACHE STRING "Location of ATL24 algorithm header-only library")
set (SLIDERULEDIR ${INSTALLDIR} CACHE STRING "Location of SlideRule library headers")

# Debug Configuration #
if(CMAKE_BUILD_TYPE MATCHES "Debug")
	target_compile_options (atl24 PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Source Files #
target_sources(atl24
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/atl24_plugin.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl24Runner.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl24Uncertainty.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl24Writer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BlunderRunner.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/PluginFields.cpp
)

# Include Directories #
target_include_directories (atl24
    PUBLIC
        ${SLIDERULEDIR}/include/sliderule
        ${ATL24DIR}/include
        ${LUA_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

# Plugin Installation #
install (
    TARGETS
        atl24 LIBRARY
    DESTINATION
        ${CONFDIR}
)
install (
    FILES
        ${ATL24DIR}/models/atl24.tgz
        ${CMAKE_CURRENT_LIST_DIR}/package/template.xml
        ${CMAKE_CURRENT_LIST_DIR}/tables/SNR_ATLAS_0_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/SNR_ATLAS_1_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/SNR_ATLAS_2_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/SNR_ATLAS_3_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/SNR_ATLAS_4_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/SNR_ATLAS_5_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/THU_ATLAS_0_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/THU_ATLAS_1_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/THU_ATLAS_2_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/THU_ATLAS_3_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/THU_ATLAS_4_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/THU_ATLAS_5_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/Transport_ATLAS_0_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/Transport_ATLAS_1_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/Transport_ATLAS_2_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/Transport_ATLAS_3_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/Transport_ATLAS_4_deg.csv
        ${CMAKE_CURRENT_LIST_DIR}/tables/Transport_ATLAS_5_deg.csv
    DESTINATION
        ${CONFDIR}
)
