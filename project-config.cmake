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

### https://clang.llvm.org/extra/clang-tidy/

    # clang-tidy, tested with version 18.1.3
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
        "-readability-avoid-const-params-in-decls,"
        "-readability-avoid-unconditional-preprocessor-if,"
        "-readability-suspicious-call-argument,"
        "-readability-isolate-declaration,"
        "-readability-misleading-indentation,"

        "misc-*,"
        "-misc-non-private-member-variables-in-classes,"
        "-misc-include-cleaner,"
        "-misc-use-anonymous-namespace,"

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
        "-hicpp-no-malloc,"

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
        "-cppcoreguidelines-virtual-class-destructor,"
        "-cppcoreguidelines-use-default-member-init,"
        "-cppcoreguidelines-avoid-non-const-global-variables,"
        "-cppcoreguidelines-pro-type-union-access,"
        "-cppcoreguidelines-interfaces-global-init,"
        "-cppcoreguidelines-no-malloc,"

        "-clang-diagnostic-unused-command-line-argument,"
    )

    # Join checks into a single string parameter
    list(JOIN CLANG_TIDY_CHECKS "" CLANG_TIDY_CHECKS_PARM)

    set (CMAKE_CXX_CLANG_TIDY
       clang-tidy;
       -header-filter=.;
       -checks=${CLANG_TIDY_CHECKS_PARM};
       -warnings-as-errors=*;
    )

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
        "--suppress=copyCtorPointerCopying:*/MsgQ.cpp"                  # line: 120, shallow copy which is fine in code
        "--error-exitcode=1"

        "--suppress=duplInheritedMember"                                # Name hiding for functions with the same name

        "--suppress=memleak:*/LuaEndpoint.cpp"                          # line: 254, 'info' is freed by requestThread but ccpcheck does not 'see it'
        "--suppress=returnDanglingLifetime:*/LuaLibraryMsg.cpp"         # line 198, code is OK
        "--suppress=constParameterReference:*/ArrowBuilderImpl.cpp"     # List [] const issue
        "--suppress=constParameterPointer:*/CcsdsPayloadDispatch.cpp"   # Not trivial to fix, would have to change DispachObject class as well.
    )

endif()

###################
# Project Options #
###################

# Project Options #

option (SHARED_LIBRARY "Create shared library instead of sliderule binary" OFF)
option (SERVER_APP "Create sliderule server binary" ON)

# Library Options #

option (ENABLE_ADDRESS_SANITIZER "Instrument code with AddressSanitizer for memory error detection" OFF)
option (ENABLE_CODE_COVERAGE "Instrument code with gcov for reporting code coverage" OFF)
option (ENABLE_TIME_HEARTBEAT "Instruct TimeLib to use a 1KHz heart beat timer to set millisecond time resolution" OFF)
option (ENABLE_CUSTOM_ALLOCATOR "Override new and delete operators globally for debug purposes" OFF)
option (ENABLE_H5CORO_ATTRIBUTE_SUPPORT "H5Coro will read and process attribute messages" OFF)
option (ENABLE_GDAL_ERROR_REPORTING "Log GDAL errors to message log" OFF)

# Package Options #

option (USE_ARROW_PACKAGE "Include the Apache Arrow package" OFF)
option (USE_AWS_PACKAGE "Include the AWS package" OFF)
option (USE_CCSDS_PACKAGE "Include the CCSDS package" ON)
option (USE_CRE_PACKAGE "Include the CRE package" OFF)
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

set(CXX_VERSION 17) # required if using pistache package

# Platform #

set_property(GLOBAL PROPERTY platform "")

if(CMAKE_BUILD_PLATFORM MATCHES "Linux")

    # Prefer libraries installed in /usr/local
    INCLUDE_DIRECTORIES(/usr/local/include)
    LINK_DIRECTORIES(/usr/local/lib /usr/local/lib64)

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
