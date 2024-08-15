message (STATUS "Including opendata dataset")

target_compile_definitions (slideruleLib PUBLIC __opendata__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/opendata.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/opendata.h
        ${CMAKE_CURRENT_LIST_DIR}/package/EsaWorldCover10meterRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/package/MetaGlobalCanopy1meterRaster.h
    DESTINATION
        ${INCDIR}/opendata
)
