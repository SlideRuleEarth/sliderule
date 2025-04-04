# Find GDAL Libraries
find_package (GDAL)

# Find PROJ Libraries
find_package (PROJ)

# Find UUID Library
find_library(LIBUUID_LIBRARY libuuid.so)

# Find TIFF Library
find_package(TIFF REQUIRED)

# Find GEOS Library
find_library(GEOS_LIBRARY libgeos_c.so)

# Build package
message (STATUS "Including geo package")

target_compile_definitions (slideruleLib PUBLIC __geo__)

target_link_libraries (slideruleLib PUBLIC ${GDAL_LIBRARIES} ${PROJ_LIBRARIES} ${LIBUUID_LIBRARY} ${GEOS_LIBRARY})
target_link_libraries(slideruleLib PUBLIC ${TIFF_LIBRARIES})
target_include_directories (slideruleLib PUBLIC ${GDAL_INCLUDE_DIRS} ${PROJ_INCLUDE_DIRS})
target_include_directories(slideruleLib PUBLIC ${TIFF_INCLUDE_DIR})

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/geo.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/DataFrameSampler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GdalRaster.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoRaster.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoIndexedRaster.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoIndexedRasterSerial.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoIndexedRasterBatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoJsonRaster.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoUserRaster.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/RasterObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/RasterSampler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/RasterSubset.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/RasterFileDictionary.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoRtree.cpp
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/package/UT_RasterSubset.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/package/UT_RasterSample.cpp>
    )

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/geo.h
        ${CMAKE_CURRENT_LIST_DIR}/package/DataFrameSampler.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GdalRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoIndexedRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoJsonRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoUserRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/package/RasterObject.h
        ${CMAKE_CURRENT_LIST_DIR}/package/RasterSampler.h
        ${CMAKE_CURRENT_LIST_DIR}/package/RasterSample.h
        ${CMAKE_CURRENT_LIST_DIR}/package/RasterSubset.h
        ${CMAKE_CURRENT_LIST_DIR}/package/RasterFileDictionary.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoRtree.h
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/package/UT_RasterSubset.h>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/package/UT_RasterSample.h>
    DESTINATION
        ${INCDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/samples.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/subsets.lua
    DESTINATION
        ${CONFDIR}/api
)
