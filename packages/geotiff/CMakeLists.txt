# Find cURL Library
find_package (TIFF)

# Build package
if (TIFF_FOUND)

    message (STATUS "Including geotiff package")

    target_compile_definitions (slideruleLib PUBLIC __geotiff__)

    target_link_libraries (slideruleLib PUBLIC ${TIFF_LIBRARIES})
    target_include_directories (slideruleLib PUBLIC ${TIFF_INCLUDE_DIR})

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/geotiff.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/geotiff.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (FATAL_ERROR "Unable to include geotiff package... required libraries not found")

endif ()
