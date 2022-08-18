# Find gdal and tiff Libraries
find_package (TIFF)
find_package (GDAL)


# Build package
if (GDAL_FOUND)

    message (STATUS "Including gdal package")

    target_compile_definitions (slideruleLib PUBLIC __gdal__)


    target_link_libraries (slideruleLib PUBLIC ${TIFF_LIBRARIES})
    target_link_libraries (slideruleLib PUBLIC ${GDAL_LIBRARIES})
    
    target_include_directories (slideruleLib PUBLIC ${TIFF_INCLUDE_DIR})
    target_include_directories (slideruleLib PUBLIC ${GDAL_INCLUDE_DIR})

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/gdalpkg.cpp
            ${CMAKE_CURRENT_LIST_DIR}/GdalRaster.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/gdalpkg.h
            ${CMAKE_CURRENT_LIST_DIR}/GdalRaster.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (FATAL_ERROR "Unable to include gdal package... required libraries not found")

endif ()
