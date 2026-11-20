message (STATUS "Including gedtm dataset")

target_compile_definitions (slideruleLib PUBLIC __gedtm__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/gedtm.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/gedtm.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GEDTM30meterRaster.h
    DESTINATION
        ${INCDIR}
)
