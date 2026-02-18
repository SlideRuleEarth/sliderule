message (STATUS "Including gedi dataset")

target_compile_definitions (slideruleLib PUBLIC __casals__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/casals.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Casals1bDataFrame.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CasalsFields.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/casals.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Casals1bDataFrame.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CasalsFields.h
    DESTINATION
        ${INCDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/casals1bx.lua
    DESTINATION
        ${CONFDIR}/api
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/crsfiles/CASALS_EPSG9989.projjson
    DESTINATION
        ${CONFDIR}
)
