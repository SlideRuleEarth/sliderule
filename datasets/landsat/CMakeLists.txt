message (STATUS "Including landsat dataset")

target_compile_definitions (slideruleLib PUBLIC __landsat__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/landsat.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LandsatHlsRaster.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/landsat.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LandsatHlsRaster.h
    DESTINATION
        ${INCDIR}/landsat
)
