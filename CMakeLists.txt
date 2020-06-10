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

option (PACKAGE_FOR_DEBIAN "Create debian installation package using cpack" ON)
option (ENABLE_ADDRESS_SANITIZER "Instrument code with AddressSanitizer for memory error detection" OFF)
option (ENABLE_TIME_HEARTBEAT "Instruct TimeLib to use a 1KHz heart beat timer to set millisecond time resolution" OFF)

option (USE_ICESAT2_PLUGIN "Include the ICESat-2 plugin" ON)
option (USE_PISTACHE_PLUGIN "Include the Pistache plugin" ON)
option (USE_SIGVIEW_PLUGIN "Include the SigView plugin" ON)

set (CMAKE_SKIP_BUILD_RPATH FALSE)
set (CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set (CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

#####################
# Target: SlideRule #
#####################

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

if (ENABLE_TIME_HEARTBEAT)
    target_compile_definitions (sliderule PUBLIC TIME_USE_HEARTBEAT)
endif ()

# Add Packages #

add_subdirectory (packages/core)
add_subdirectory (packages/ccsds)
add_subdirectory (packages/legacy)

# Add Optional Plugins #

if(${USE_ICESAT2_PLUGIN})
    add_subdirectory (plugins/icesat2)
endif()

if(${USE_PISTACHE_PLUGIN})
    add_subdirectory (plugins/pistache)
endif()

if(${USE_SIGVIEW_PLUGIN})
    add_subdirectory (plugins/sigview)
endif()

# Scripts #

add_subdirectory (scripts)

# Installation #

install(TARGETS sliderule DESTINATION ${EXECDIR})

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


