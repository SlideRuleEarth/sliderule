# SlideRule top-level CMake build script

project (SLIDERULE LANGUAGES CXX)

#################
# CMake Options #
#################

set (CMAKE_LEGACY_CYGWIN_WIN32 0) # Squelch a warning when building on Win32/Cygwin
cmake_minimum_required (VERSION 3.13.0) # The minimum CMake version is chosen to enable policy CMP0079
cmake_policy(SET CMP0079 NEW) # add link library to target which is not built in this directory
cmake_policy(SET CMP0053 NEW) # simplified variable escape processing (recommended by cmake)

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

###################
# Project Options #
###################

option (PACKAGE_FOR_DEBIAN "Create debian installation package using cpack" OFF)
option (ENABLE_ADDRESS_SANITIZER "Instrument code with AddressSanitizer for memory error detection" OFF)
option (ENABLE_TIME_HEARTBEAT "Instruct TimeLib to use a 1KHz heart beat timer to set millisecond time resolution" OFF)
option (ENABLE_CUSTOM_ALLOCATOR "Override new and delete operators globally for debug purposes" OFF)
option (ENABLE_H5_REST_VOL "Install and use the REST VOL plugin for H5 access" OFF)
option (ENABLE_LTTNG_TRACING "Instantiate LTTng trace points" OFF)

option (USE_AWS_PACKAGE "Include the AWS package" OFF)
option (USE_CCSDS_PACKAGE "Include the CCSDS package" ON)
option (USE_H5_PACKAGE "Include the HDF5 package" OFF)
option (USE_PISTACHE_PACKAGE "Include the Pistache package" OFF)
option (USE_MONGOOSE_PACKAGE "Include the Mongoose package" OFF)
option (USE_LEGACY_PACKAGE "Include the Legacy package" ON)

option (USE_ICESAT2_PLUGIN "Include the ICESat-2 plugin" OFF)
option (USE_SIGVIEW_PLUGIN "Include the SigView plugin" ON)

set (CMAKE_SKIP_BUILD_RPATH FALSE)
set (CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set (CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

#####################
# Target: SlideRule #
#####################

# Create Target #

add_executable (sliderule ${PROJECT_SOURCE_DIR}/targets/sliderule-linux/SlideRule.cpp)

# Version Information #

file(STRINGS ${PROJECT_SOURCE_DIR}/version.txt TGTVER)

# Platform #

if(CMAKE_BUILD_PLATFORM MATCHES "Linux")

    # Prefer libraries installed in /usr/local
    INCLUDE_DIRECTORIES(/usr/local/include)
    LINK_DIRECTORIES(/usr/local/lib)

    # Set Environment Variables
    set (INSTALLDIR /usr/local CACHE STRING "Installation directory for library and executables")
    set (CONFDIR ${INSTALLDIR}/etc/sliderule)
    set (EXECDIR ${INSTALLDIR}/bin)
    set (INCDIR ${INSTALLDIR}/include/sliderule)
    set (TOOLCHAIN _GNU_ CACHE STRING "Toolchain used to build code") # need to rework so that compile tool chain is somehow detected
    set (ENDIAN __LE__ CACHE STRING "Endianess of local machine")  # ideally moved to some architecture specific file

    add_subdirectory (platforms/linux)

elseif(CMAKE_BUILD_PLATFORM MATCHES "Windows")

    # Set Environment Variables
    set (INSTALLDIR "C:\\Program Files\\SlideRule" CACHE STRING "Installation directory for library and executables")
    set (CONFDIR ${INSTALLDIR}/etc/sliderule)
    set (EXECDIR ${INSTALLDIR}/bin)
    set (INCDIR ${INSTALLDIR}/include/sliderule)
    set (TOOLCHAIN _VS_ CACHE STRING "Toolchain used to build code") # need to rework so that compile tool chain is somehow detected
    set (ENDIAN __LE__ CACHE STRING "Endianess of local machine")  # ideally moved to some architecture specific file

    add_subdirectory (platforms/windows)

endif()

# Target Properties #

set_target_properties (sliderule PROPERTIES VERSION ${TGTVER})
set_target_properties (sliderule PROPERTIES OUTPUT_NAME sliderule)
set_target_properties (sliderule PROPERTIES ENABLE_EXPORTS true)

# Compile Definitions #

set (RUNTIMEDIR ${CONFDIR} CACHE STRING "Runtime directory for plugins and configuration scripts")

target_compile_definitions (sliderule PUBLIC BINID="${TGTVER}")
target_compile_definitions (sliderule PUBLIC CONFIGPATH="${RUNTIMEDIR}")
target_compile_definitions (sliderule PUBLIC ${TOOLCHAIN})
target_compile_definitions (sliderule PUBLIC ${ENDIAN})

if (${ENABLE_TIME_HEARTBEAT})
    message (STATUS "Enabling time heartbeat")
    target_compile_definitions (sliderule PUBLIC TIME_USE_HEARTBEAT)
endif ()

if (${ENABLE_CUSTOM_ALLOCATOR})
    message (STATUS "Enabling custom allocator")
    target_compile_definitions (sliderule PUBLIC CUSTOM_ALLOCATOR)
endif ()

if (DEFINED MAX_STR_SIZE)
    message (STATUS "Setting MAX_STR_SIZE to " ${MAX_STR_SIZE})
    target_compile_definitions (sliderule PUBLIC MAX_STR_SIZE=${MAX_STR_SIZE})
endif ()

if (DEFINED MAX_FREE_STACK_SIZE)
    message (STATUS "Setting MAX_FREE_STACK_SIZE to " ${MAX_FREE_STACK_SIZE})
    target_compile_definitions (sliderule PUBLIC MAX_FREE_STACK_SIZE=${MAX_FREE_STACK_SIZE})
endif ()

if (DEFINED LIST_BLOCK_SIZE)
    message (STATUS "Setting LIST_BLOCK_SIZE to " ${LIST_BLOCK_SIZE})
    target_compile_definitions (sliderule PUBLIC LIST_BLOCK_SIZE=${LIST_BLOCK_SIZE})
endif ()

# Add Packages #

add_subdirectory (packages/core)

if(${USE_AWS_PACKAGE})
    add_subdirectory (packages/aws)
endif()

if(${USE_CCSDS_PACKAGE})
    add_subdirectory (packages/ccsds)
endif()

if(${USE_H5_PACKAGE})
    add_subdirectory (packages/h5)
endif()

if(${USE_PISTACHE_PACKAGE})
    add_subdirectory (packages/pistache)
endif()

if(${USE_MONGOOSE_PACKAGE})
    add_subdirectory (packages/mongoose)
endif()

if(${USE_LEGACY_PACKAGE})
    add_subdirectory (packages/legacy)
endif()

# Add Plugins #

if(${USE_ICESAT2_PLUGIN})
    add_subdirectory (plugins/icesat2)
endif()

if(${USE_SIGVIEW_PLUGIN})
    add_subdirectory (plugins/sigview)
endif()

# Scripts #

add_subdirectory (scripts)

# Installation #

install (TARGETS sliderule DESTINATION ${EXECDIR})
install (FILES targets/sliderule-linux/plugins.conf DESTINATION ${CONFDIR})

install (
    DIRECTORY
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    DESTINATION ${CONFDIR}
)

# Debian Package #

if(${PACKAGE_FOR_DEBIAN})
	set (CPACK_GENERATOR "DEB")
	set (CPACK_DEBIAN_PACKAGE_MAINTAINER "JP Swinski") # required
	set (CPACK_DEBIAN_PACKAGE_DESCRIPTION "a framework for on-demand science data processing")
	set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    string (REGEX MATCHALL "[0123456789]+" SLIDERULE_VERSION_NUMBERS "${TGTVER}")
	list (GET SLIDERULE_VERSION_NUMBERS 0 CPACK_PACKAGE_VERSION_MAJOR)
	list (GET SLIDERULE_VERSION_NUMBERS 1 CPACK_PACKAGE_VERSION_MINOR)
	list (GET SLIDERULE_VERSION_NUMBERS 2 CPACK_PACKAGE_VERSION_PATCH)
	set (SLIDERULE_PACKAGE_VERSION "sliderule-VERSION.deb")
	string (REGEX REPLACE "VERSION" "${TGTVER}" SLIDERULE_PACKAGE_VERSION "${SLIDERULE_PACKAGE_VERSION}")
	set (CPACK_DEBIAN_FILE_NAME ${SLIDERULE_PACKAGE_VERSION})
	include (CPack)
endif()


