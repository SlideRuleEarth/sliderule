# Find GDAL Libraries
find_package (GDAL)

# Find PROJ Libraries
find_package (PROJ)

# Find UUID Library
find_library(LIBUUID_LIBRARY libuuid.so)

# Build package
if (GDAL_FOUND AND PROJ_FOUND)

    message (STATUS "Including geo package")

    target_compile_definitions (slideruleLib PUBLIC __geo__)

    target_link_libraries (slideruleLib PUBLIC ${GDAL_LIBRARIES} ${PROJ_LIBRARIES} ${LIBUUID_LIBRARY})
    target_include_directories (slideruleLib PUBLIC ${GDAL_INCLUDE_DIRS} ${PROJ_INCLUDE_DIRS})

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/geo.cpp
            ${CMAKE_CURRENT_LIST_DIR}/GdalRaster.cpp
            ${CMAKE_CURRENT_LIST_DIR}/GeoRaster.cpp
            ${CMAKE_CURRENT_LIST_DIR}/GeoIndexedRaster.cpp
            ${CMAKE_CURRENT_LIST_DIR}/GeoJsonRaster.cpp
            ${CMAKE_CURRENT_LIST_DIR}/GeoUserRaster.cpp
            ${CMAKE_CURRENT_LIST_DIR}/RasterObject.cpp
            ${CMAKE_CURRENT_LIST_DIR}/RasterSampler.cpp
            ${CMAKE_CURRENT_LIST_DIR}/GeoParms.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/geo.h
            ${CMAKE_CURRENT_LIST_DIR}/GdalRaster.h
            ${CMAKE_CURRENT_LIST_DIR}/GeoRaster.h
            ${CMAKE_CURRENT_LIST_DIR}/GeoIndexedRaster.h
            ${CMAKE_CURRENT_LIST_DIR}/GeoJsonRaster.h
            ${CMAKE_CURRENT_LIST_DIR}/GeoUserRaster.h
            ${CMAKE_CURRENT_LIST_DIR}/RasterObject.h
            ${CMAKE_CURRENT_LIST_DIR}/RasterSampler.h
            ${CMAKE_CURRENT_LIST_DIR}/RasterSample.h
            ${CMAKE_CURRENT_LIST_DIR}/GeoParms.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (FATAL_ERROR "Unable to include geo package... required libraries not found")

endif ()
