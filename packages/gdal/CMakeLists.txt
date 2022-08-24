# Find gdal Libraries
find_package (GDAL)

find_package(PkgConfig)
pkg_check_modules(UUID uuid)

# Build package
if (GDAL_FOUND)

    message (STATUS "Including gdal package")

    target_compile_definitions (slideruleLib PUBLIC __gdal__)


    target_link_libraries (slideruleLib PUBLIC ${GDAL_LIBRARIES} ${UUID_LIBRARIES})
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
