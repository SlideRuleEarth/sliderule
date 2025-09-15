cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
project (SLIDERULE LANGUAGES CXX)

# Squelch a warning when building on Win32/Cygwin
set (CMAKE_LEGACY_CYGWIN_WIN32 0)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(default_build_type "Release")
    if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
      set(default_build_type "Debug")
    endif()
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
endif()

# Set a default platform
if(NOT CMAKE_BUILD_PLATFORM)
    set(CMAKE_BUILD_PLATFORM "Linux" CACHE STRING "Choose the target platform." FORCE)
endif()

# Configure static analysis
if(CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT SKIP_STATIC_ANALYSIS)
    message(STATUS "Enabling static analysis")

    # clang-tidy, tested with version 18.1.3
    # https://clang.llvm.org/extra/clang-tidy/
    set (CLANG_TIDY_CHECKS
        "clang-analyzer-*,"
        "portability-*,"
        "concurrency-*,"   # Several warnings were disabled in cpp files with NOLINT

        "performance-*,"
        "-performance-enum-size,"

        "readability-*,"
        "-readability-braces-around-statements,"
        "-readability-implicit-bool-conversion,"
        "-readability-magic-numbers,"
        "-readability-function-cognitive-complexity,"
        "-readability-identifier-length,"
        "-readability-simplify-boolean-expr,"
        "-readability-else-after-return,"
        "-readability-suspicious-call-argument,"
        "-readability-isolate-declaration,"

        "misc-*,"
        "-misc-non-private-member-variables-in-classes,"

        "hicpp-*,"
        "-hicpp-special-member-functions,"
        "-hicpp-use-nullptr,"
        "-hicpp-use-noexcept,"
        "-hicpp-avoid-c-arrays,"
        "-hicpp-member-init,"
        "-hicpp-vararg,"
        "-hicpp-no-array-decay,"
        "-hicpp-braces-around-statements,"
        "-hicpp-use-auto,"
        "-hicpp-deprecated-headers,"
        "-hicpp-signed-bitwise,"

        # Most are turned off but several are very useful - checks for correct c++ casting, c style stuff misc and hicpp missed
        "cppcoreguidelines-*,"
        "-cppcoreguidelines-avoid-c-arrays,"
        "-cppcoreguidelines-avoid-magic-numbers,"
        "-cppcoreguidelines-avoid-const-or-ref-data-members,"
        "-cppcoreguidelines-macro-usage,"
        "-cppcoreguidelines-pro-bounds-array-to-pointer-decay,"
        "-cppcoreguidelines-pro-type-vararg,"
        "-cppcoreguidelines-pro-bounds-constant-array-index,"
        "-cppcoreguidelines-macro-to-enum,"
        "-cppcoreguidelines-narrowing-conversions,"
        "-cppcoreguidelines-special-member-functions,"
        "-cppcoreguidelines-non-private-member-variables-in-classes,"
        "-cppcoreguidelines-pro-bounds-pointer-arithmetic,"
        "-cppcoreguidelines-owning-memory,"
        "-cppcoreguidelines-pro-type-member-init,"
        "-cppcoreguidelines-init-variables,"
        "-cppcoreguidelines-avoid-do-while,"
        "-cppcoreguidelines-prefer-member-initializer,"
        "-cppcoreguidelines-pro-type-const-cast,"
        "-cppcoreguidelines-pro-type-reinterpret-cast,"
        "-cppcoreguidelines-avoid-non-const-global-variables,"
        "-cppcoreguidelines-pro-type-union-access,"
        "-cppcoreguidelines-interfaces-global-init,"

        "-clang-diagnostic-unused-command-line-argument,"
    )

    # Join checks into a single string parameter
    list(JOIN CLANG_TIDY_CHECKS "" CLANG_TIDY_CHECKS_PARM)

    set (CMAKE_CXX_CLANG_TIDY "clang-tidy;-header-filter='^((?!ATL24_coastnet/).)*$';-checks=${CLANG_TIDY_CHECKS_PARM};-warnings-as-errors=*")

    # cppcheck, tested with version 2.13.0
    find_program (CMAKE_CXX_CPPCHECK NAMES cppcheck)
    list (APPEND CMAKE_CXX_CPPCHECK
        "--quiet"
        "--enable=all"
        "--suppress=missingInclude"
        "--suppress=missingIncludeSystem"
        "--suppress=unmatchedSuppression"
        "--suppress=unusedFunction"                                     # cppcheck is confused, used functions are reported 'unused'
        "--suppress=unusedPrivateFunction"                              # cppcheck is confused
        "--suppress=noOperatorEq"
        "--suppress=noCopyConstructor"
        "--suppress=useStlAlgorithm"                                    # yeah, they may be a bit faster but code is unreadable
        "--suppress=memsetClassFloat:*/MathLib.cpp"                     # line: 80, need memset here for performance
        "--suppress=unreadVariable:*/TimeLib.cpp"                       # line: 471, terminating '\0' detected as 'unused' but it is used/needed
        "--suppress=invalidPointerCast:*/H5Array.h"                     # line: 166, documented in code
        "--suppress=invalidPointerCast:*/H5Element.h"                   # line: 141, documented in code
        "--suppress=invalidPointerCast:*/H5Coro.cpp"                    # info.data is newed on 8 byte boundries, can supress this warning
        "--suppress=uninitvar:*/H5Coro.cpp"                             # info
        "--suppress=copyCtorPointerCopying:*/MsgQ.cpp"                  # line: 120, shallow copy which is fine in code
        "--suppress=invalidPointerCast:*/GeoLib.cpp"                    # TIFFImage.raster is newed on 8 byte boundries, can supress this warning
        "--error-exitcode=1"

        "--suppress=duplInheritedMember"                                # Name hiding for functions with the same name

        "--suppress=memleak:*/LuaEndpoint.cpp"                          # line: 254, 'info' is freed by requestThread but ccpcheck does not 'see it'
        "--suppress=returnDanglingLifetime:*/LuaLibraryMsg.cpp"         # line 198, code is OK
        "--suppress=constParameterReference:*/ArrowBuilderImpl.cpp"     # List [] const issue
        "--suppress=constParameterReference:*/BathyRefractionCorrector.cpp" # in run() param extent cannot be const
        "--suppress=constParameterPointer:*/CcsdsPayloadDispatch.cpp"   # Not trivial to fix, would have to change DispachObject class as well.
        "--suppress=knownConditionTrueFalse"                            # -1 < 0 etc
        "--suppress=constVariableReference:*/RasterObject.cpp"          # List [] const issue
        "--suppress=constVariableReference:*/GeoIndexedRaster.cpp"      # List [] const issue
    )

endif()

###################
# Project Options #
###################

# Project Options #

option (SHARED_LIBRARY "Create shared library instead of sliderule binary" OFF)

# Library Options #

option (ENABLE_ADDRESS_SANITIZER "Instrument code with AddressSanitizer for memory error detection" OFF)
option (ENABLE_CODE_COVERAGE "Instrument code with gcov for reporting code coverage" OFF)
option (ENABLE_TIME_HEARTBEAT "Instruct TimeLib to use a 1KHz heart beat timer to set millisecond time resolution" OFF)
option (ENABLE_CUSTOM_ALLOCATOR "Override new and delete operators globally for debug purposes" OFF)
option (ENABLE_H5CORO_ATTRIBUTE_SUPPORT "H5Coro will read and process attribute messages" OFF)
option (ENABLE_GDAL_ERROR_REPORTING "Log GDAL errors to message log" OFF)

# Package Options #

option (USE_ARROW_PACKAGE "Include the Apache Arrow package" ON)
option (USE_AWS_PACKAGE "Include the AWS package" ON)
option (USE_CCSDS_PACKAGE "Include the CCSDS package" OFF)
option (USE_CRE_PACKAGE "Include the container runtime environment package" ON)
option (USE_GEO_PACKAGE "Include the geospatial library (GDAL) package" ON)
option (USE_H5_PACKAGE "Include the H5Coro package" ON)
option (USE_HDF_PACKAGE "Include the HDFGroup package" OFF)
option (USE_LEGACY_PACKAGE "Include the legacy (CommandProcessor) package" OFF)
option (USE_STREAMING_PACKAGE "Include the streaming (RecordDispatcher) package" ON)

# Platform Options #

option (ENABLE_TRACING "Instantiate trace points" OFF)
option (ENABLE_TERMINAL "Instantiate terminal messages" ON)

# Dataset Options #

option (USE_BATHY_DATASET "Include the BATHY dataset" ON)
option (USE_BLUETOPO_DATASET "Include the BLUETOPO dataset" ON)
option (USE_GEBCO_DATASET "Include the GEBCO dataset" ON)
option (USE_GEDI_DATASET "Include the GEDI dataset" ON)
option (USE_ICESAT2_DATASET "Include the ICESat2 dataset" ON)
option (USE_LANDSAT_DATASET "Include the LandSat dataset" ON)
option (USE_OPENDATA_DATASET "Include the OpenData dataset" ON)
option (USE_PGC_DATASET "Include the PGC dataset" ON)
option (USE_SWOT_DATASET "Include the SWOT dataset" ON)
option (USE_USGS3DEP_DATASET "Include the USGS 3DEP dataset" ON)
option (USE_GEDTM_DATASET "Include the GEDTM dataset" ON)
option (USE_NISAR_DATASET "Include the NISAR dataset" ON)

# Target Options #

option (USE_SLIDERULEEARTH_TARGET "Include slideruleearth.io target configuration" ON)

# Version Information #

file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/version.txt TGTVER)
string(REPLACE "v" "" LIBVER ${TGTVER})

# C++ Version #

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

# Platform #

set_property(GLOBAL PROPERTY platform "")

if(CMAKE_BUILD_PLATFORM MATCHES "Linux")

    # Prefer libraries installed in /usr/local
    INCLUDE_DIRECTORIES(/usr/local/include)
    LINK_DIRECTORIES(/usr/local/lib /usr/local/lib64)

    # Set Environment Variables
    set (INSTALLDIR /usr/local CACHE STRING "Installation prefix directory for sliderule application")
    set (CONFDIR ${INSTALLDIR}/etc/sliderule)
    set (INCDIR ${INSTALLDIR}/include/sliderule)

endif()

#####################
# Target: SlideRule #
#####################

# SlideRule Library #

if(${SHARED_LIBRARY})
    add_library(slideruleLib SHARED "")
else()
    add_library(slideruleLib STATIC "")
endif()

# Platform Config #

if(CMAKE_BUILD_PLATFORM MATCHES "Linux")
    add_subdirectory (platforms/linux)
endif()

# Target Properties #

set_target_properties (slideruleLib PROPERTIES VERSION ${LIBVER})
set_target_properties (slideruleLib PROPERTIES OUTPUT_NAME sliderule)
set_target_properties (slideruleLib PROPERTIES ENABLE_EXPORTS true)

# Compile Definitions #

if (${ENABLE_TIME_HEARTBEAT})
    message (STATUS "Enabling time heartbeat")
    target_compile_definitions (slideruleLib PUBLIC TIME_HEARTBEAT)
endif ()

if (${ENABLE_CUSTOM_ALLOCATOR})
    message (STATUS "Enabling custom allocator")
    target_compile_definitions (slideruleLib PUBLIC CUSTOM_ALLOCATOR)
endif ()

if (DEFINED MAX_FREE_STACK_SIZE)
    message (STATUS "Setting MAX_FREE_STACK_SIZE to " ${MAX_FREE_STACK_SIZE})
    target_compile_definitions (slideruleLib PUBLIC MAX_FREE_STACK_SIZE=${MAX_FREE_STACK_SIZE})
endif ()

if (DEFINED H5CORO_THREAD_POOL_SIZE)
    message (STATUS "Setting H5CORO_THREAD_POOL_SIZE to " ${H5CORO_THREAD_POOL_SIZE})
    target_compile_definitions (slideruleLib PUBLIC H5CORO_THREAD_POOL_SIZE=${H5CORO_THREAD_POOL_SIZE})
endif ()

if (DEFINED H5CORO_MAXIMUM_NAME_SIZE)
    message (STATUS "Setting H5CORO_MAXIMUM_NAME_SIZE to " ${H5CORO_MAXIMUM_NAME_SIZE})
    target_compile_definitions (slideruleLib PUBLIC H5CORO_MAXIMUM_NAME_SIZE=${H5CORO_MAXIMUM_NAME_SIZE})
endif ()

if (${ENABLE_H5CORO_ATTRIBUTE_SUPPORT})
    message (STATUS "Enabling support in H5Coro for attribute messages")
    target_compile_definitions (slideruleLib PUBLIC H5CORO_ATTRIBUTE_SUPPORT)
endif ()

if (${ENABLE_GDAL_ERROR_REPORTING})
    message (STATUS "Enabling error reporting to message log")
    target_compile_definitions (slideruleLib PUBLIC GDAL_ERROR_REPORTING)
endif ()

# Platform File #

set_property(GLOBAL APPEND PROPERTY platform "#define LIBID \"${TGTVER}\"\n")
set_property(GLOBAL APPEND PROPERTY platform "#define CONFDIR \"${CONFDIR}\"\n")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_property(GLOBAL APPEND PROPERTY platform "#define __unittesting__\n")
endif()

if(${ENABLE_TRACING})
    message (STATUS "Enabling trace points")
    set_property(GLOBAL APPEND PROPERTY platform "#define __tracing__\n")
endif()

if(${ENABLE_TERMINAL})
    message (STATUS "Enabling terminal messages")
    set_property(GLOBAL APPEND PROPERTY platform "#define __terminal__\n")
endif()

include (TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    message (STATUS "Enabling code compilation for big endian machine")
    set_property(GLOBAL APPEND PROPERTY platform "#define __be__\n")
else()
    message (STATUS "Enabling code compilation for little endian machine")
    set_property(GLOBAL APPEND PROPERTY platform "#define __le__\n")
endif()

get_property(PLATFORM GLOBAL PROPERTY platform)
file (WRITE ${CMAKE_CURRENT_BINARY_DIR}/auto/platform.h ${PLATFORM})

target_include_directories (slideruleLib PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/auto)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/auto/platform.h DESTINATION ${INCDIR})

# Add Packages #

add_subdirectory (packages/core)

if(${USE_ARROW_PACKAGE})
    add_subdirectory (packages/arrow)
endif()

if(${USE_AWS_PACKAGE})
    add_subdirectory (packages/aws)
endif()

if(${USE_CCSDS_PACKAGE})
    add_subdirectory (packages/ccsds)
endif()

if(${USE_CRE_PACKAGE})
    add_subdirectory (packages/cre)
endif()

if(${USE_GEO_PACKAGE})
    add_subdirectory (packages/geo)
endif()

if(${USE_H5_PACKAGE})
    add_subdirectory (packages/h5)
endif()

if(${USE_HDF_PACKAGE})
    add_subdirectory (packages/hdf)
endif()

if(${USE_LEGACY_PACKAGE})
    add_subdirectory (packages/legacy)
endif()

if(${USE_STREAMING_PACKAGE})
    add_subdirectory (packages/streaming)
endif()

# Datasets #

if(${USE_BATHY_DATASET})
    add_subdirectory (datasets/bathy)
endif()

if(${USE_BLUETOPO_DATASET})
    add_subdirectory (datasets/bluetopo)
endif()

if(${USE_GEBCO_DATASET})
    add_subdirectory (datasets/gebco)
endif()

if(${USE_GEDI_DATASET})
    add_subdirectory (datasets/gedi)
endif()

if(${USE_ICESAT2_DATASET})
    add_subdirectory (datasets/icesat2)
endif()

if(${USE_LANDSAT_DATASET})
    add_subdirectory (datasets/landsat)
endif()

if(${USE_OPENDATA_DATASET})
    add_subdirectory (datasets/opendata)
endif()

if(${USE_PGC_DATASET})
    add_subdirectory (datasets/pgc)
endif()

if(${USE_SWOT_DATASET})
    add_subdirectory (datasets/swot)
endif()

if(${USE_USGS3DEP_DATASET})
    add_subdirectory (datasets/usgs3dep)
endif()

if(${USE_GEDTM_DATASET})
    add_subdirectory (datasets/gedtm)
endif()

if(${USE_NISAR_DATASET})
    add_subdirectory (datasets/nisar)
endif()

# Targets #

if(${USE_SLIDERULEEARTH_TARGET})
    add_subdirectory (targets/slideruleearth)
endif()

# Installation #

install (TARGETS slideruleLib DESTINATION ${INSTALLDIR}/lib)
install (FILES ${CMAKE_CURRENT_LIST_DIR}/version.txt DESTINATION ${CONFDIR})
