#################
# CMake Options #
#################

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
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "Enabling static analysis")

    # clang-tidy
    set (
        CMAKE_CXX_CLANG_TIDY clang-tidy;
        -header-filter=.;
        -checks=clang-analyzer-*,concurrency-*,misc-*,performance-*,portability-*,readability-*,-readability-braces-around-statements,-readability-implicit-bool-conversion,-readability-magic-numbers,-misc-non-private-member-variables-in-classes;
        -warnings-as-errors=*;
    )

    # cppcheck
#   find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
#   list(APPEND CMAKE_CXX_CPPCHECK "--enable=all")
endif()

###################
# Project Options #
###################

# Project Options #

option (PYTHON_BINDINGS "Create Python bindings, including h5lite module" OFF)
option (SHARED_LIBRARY "Create shared library instead of sliderule binary" OFF)
option (SERVER_APP "Create sliderule server binary" ON)

# Library Options #

option (ENABLE_COMPAT "Use C++11 for compatibility with older compilers" OFF)
option (ENABLE_ADDRESS_SANITIZER "Instrument code with AddressSanitizer for memory error detection" OFF)
option (ENABLE_TIME_HEARTBEAT "Instruct TimeLib to use a 1KHz heart beat timer to set millisecond time resolution" OFF)
option (ENABLE_CUSTOM_ALLOCATOR "Override new and delete operators globally for debug purposes" OFF)
option (ENABLE_H5CORO_ATTRIBUTE_SUPPORT "H5Coro will read and process attribute messages" OFF)
option (ENABLE_APACHE_ARROW_10_COMPAT "Use Apache Arrow 11 interface" OFF)
option (ENABLE_BEST_EFFORT_CONDA_ENV "Attempt to alleviate some issues with running in a conda environment")

# Package Options #

option (USE_ARROW_PACKAGE "Include the Apache Arrow package" OFF)
option (USE_AWS_PACKAGE "Include the AWS package" OFF)
option (USE_CCSDS_PACKAGE "Include the CCSDS package" ON)
option (USE_GEO_PACKAGE "Include the GEO package" OFF)
option (USE_H5_PACKAGE "Include the HDF5 package" ON)
option (USE_LEGACY_PACKAGE "Include the Legacy package" ON)
option (USE_NETSVC_PACKAGE "Include the Network Services package" OFF)
option (USE_PISTACHE_PACKAGE "Include the Pistache package" OFF)

# Platform Options #

option (ENABLE_TRACING "Instantiate trace points" OFF)
option (ENABLE_TERMINAL "Instantiate terminal messages" ON)

# Version Information #

file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/version.txt TGTVER)
string(REPLACE "v" "" LIBVER ${TGTVER})

# C++ Version #

if(${ENABLE_COMPAT})
    set(CXX_VERSION 11)
else()
    set(CXX_VERSION 17) # required if using pistache package
endif()

# Platform #

set_property(GLOBAL PROPERTY platform "")

if(CMAKE_BUILD_PLATFORM MATCHES "Linux")

    # Prefer libraries installed in /usr/local
    INCLUDE_DIRECTORIES(/usr/local/include)
    LINK_DIRECTORIES(/usr/local/lib)

    # Set Environment Variables
    set (INSTALLDIR /usr/local CACHE STRING "Installation directory for library and executables")
    set (CONFDIR ${INSTALLDIR}/etc/sliderule)
    set (INCDIR ${INSTALLDIR}/include/sliderule)

elseif(CMAKE_BUILD_PLATFORM MATCHES "Windows")

    # Set Environment Variables
    set (INSTALLDIR "C:\\Program Files\\SlideRule" CACHE STRING "Installation directory for library and executables")
    set (CONFDIR ${INSTALLDIR}\etc\sliderule)
    set (INCDIR ${INSTALLDIR}\include\sliderule)

endif()

set (RUNTIMEDIR ${CONFDIR} CACHE STRING "Runtime directory for plugins and configuration scripts")
