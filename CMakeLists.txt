# SlideRule top-level CMake build script

project (SLIDERULE LANGUAGES CXX)
cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
include(${CMAKE_CURRENT_SOURCE_DIR}/project-config.cmake)

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
elseif(CMAKE_BUILD_PLATFORM MATCHES "Windows")
    add_subdirectory (platforms/windows)
endif()

# Target Properties #

set_target_properties (slideruleLib PROPERTIES VERSION ${LIBVER})
set_target_properties (slideruleLib PROPERTIES OUTPUT_NAME sliderule)
set_target_properties (slideruleLib PROPERTIES ENABLE_EXPORTS true)
set_target_properties (slideruleLib PROPERTIES CXX_STANDARD ${CXX_VERSION})

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

if (${ENABLE_BEST_EFFORT_CONDA_ENV})
    message (STATUS "Attempting best effort at running in a mixed system and conda environment")
    target_compile_definitions (slideruleLib PUBLIC BEST_EFFORT_CONDA_ENV)
endif ()

# Platform File #

set_property(GLOBAL APPEND PROPERTY platform "#define LIBID \"${TGTVER}\"\n")
set_property(GLOBAL APPEND PROPERTY platform "#define CONFDIR \"${RUNTIMEDIR}\"\n")

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

if(${USE_LEGACY_PACKAGE})
    add_subdirectory (packages/legacy)
endif()

if(${USE_NETSVC_PACKAGE})
    add_subdirectory (packages/netsvc)
endif()

if(${USE_PISTACHE_PACKAGE})
    add_subdirectory (packages/pistache)
endif()

# Scripts #

add_subdirectory (scripts)

# Executables #

install (TARGETS slideruleLib DESTINATION ${INSTALLDIR}/lib)

if(${SERVER_APP} AND (CMAKE_BUILD_PLATFORM MATCHES "Linux"))
    add_subdirectory (targets/server-linux)
endif()
