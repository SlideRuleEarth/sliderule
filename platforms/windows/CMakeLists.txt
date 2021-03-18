message (STATUS "Using windows platform")

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

