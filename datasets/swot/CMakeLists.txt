message (STATUS "Including swot dataset")

target_compile_definitions (slideruleLib PUBLIC __swot__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/swot.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/SwotFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/SwotL2Reader.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/swot.h
        ${CMAKE_CURRENT_LIST_DIR}/package/SwotFields.h
        ${CMAKE_CURRENT_LIST_DIR}/package/SwotL2Reader.h
    DESTINATION
        ${INCDIR}/swot
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/swotl2.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/swotl2p.lua
    DESTINATION
        ${CONFDIR}/api
)
