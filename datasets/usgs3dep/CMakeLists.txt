message (STATUS "Including usgs3dep dataset")

target_compile_definitions (slideruleLib PUBLIC __usgs3dep__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/usgs3dep.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Usgs3dep1meterDemRaster.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Usgs3dep10meterDemRaster.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/usgs3dep.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Usgs3dep1meterDemRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Usgs3dep10meterDemRaster.h
    DESTINATION
        ${INCDIR}
)
