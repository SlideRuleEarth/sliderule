message (STATUS "Using windows platform")

target_compile_definitions (slideruleLib PUBLIC _WINDOWS_)

# Version Information #

execute_process(COMMAND type ${PROJECT_SOURCE_DIR}\version.txt OUTPUT_VARIABLE TGTVER)
string(REGEX REPLACE "\n$" "" TGTVER "${TGTVER}")

target_compile_definitions (slideruleLib PUBLIC BINID="${TGTVER}")

set_target_properties (slideruleLib PROPERTIES VERSION ${TGTVER})

# Compiler Options #

# Linker Options #

# Sources #

target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/OsApi.cpp
)

# Includes #

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

# Installs #

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/OsApi.h
    DESTINATION
        ${INCDIR}
)

