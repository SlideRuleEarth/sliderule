message (STATUS "Including pgc dataset")

target_compile_definitions (slideruleLib PUBLIC __pgc__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/pgc.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/PgcWkt.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/PgcDemStripsRaster.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/pgc.h
        ${CMAKE_CURRENT_LIST_DIR}/package/PgcWkt.h
        ${CMAKE_CURRENT_LIST_DIR}/package/PgcDemStripsRaster.h
    DESTINATION
        ${INCDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/ITRF2014_3031.wkt
        ${CMAKE_CURRENT_LIST_DIR}/package/ITRF2014_3413.wkt
    DESTINATION
        ${CONFDIR}
)