message (STATUS "Including gebco dataset")

target_compile_definitions (slideruleLib PUBLIC __gebco__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/gebco.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GebcoBathyRaster.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/gebco.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GebcoBathyRaster.h
    DESTINATION
        ${INCDIR}
)
