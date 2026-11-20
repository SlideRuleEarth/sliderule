message (STATUS "Including bluetopo dataset")

target_compile_definitions (slideruleLib PUBLIC __bluetopo__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/bluetopo.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BlueTopoBathyRaster.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/bluetopo.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BlueTopoBathyRaster.h
    DESTINATION
        ${INCDIR}
)
