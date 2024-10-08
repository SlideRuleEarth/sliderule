message (STATUS "Including gedi dataset")

target_compile_definitions (slideruleLib PUBLIC __gedi__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/gedi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Gedi01bReader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Gedi02aReader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Gedi04aReader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GediFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GediIODriver.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/gedi.h
        ${CMAKE_CURRENT_LIST_DIR}/package/FootprintReader.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Gedi01bReader.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Gedi02aReader.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Gedi03Raster.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Gedi04aReader.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Gedi04bRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GediFields.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GediIODriver.h
    DESTINATION
        ${INCDIR}/gedi
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi01b.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi01bp.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi02a.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi02ap.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi04a.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/gedi04ap.lua
    DESTINATION
        ${CONFDIR}/api
)
