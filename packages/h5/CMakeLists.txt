find_package (ZLIB)

# Build package
if (ZLIB_FOUND)

    message (STATUS "Including h5 package")

    target_compile_definitions (slideruleLib PUBLIC __h5__)

    target_link_libraries (slideruleLib PUBLIC ${ZLIB_LIBRARIES})

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/h5.cpp
            ${CMAKE_CURRENT_LIST_DIR}/H5File.cpp
            ${CMAKE_CURRENT_LIST_DIR}/H5DatasetDevice.cpp
            ${CMAKE_CURRENT_LIST_DIR}/H5Coro.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/h5.h
            ${CMAKE_CURRENT_LIST_DIR}/H5Array.h
            ${CMAKE_CURRENT_LIST_DIR}/H5File.h
            ${CMAKE_CURRENT_LIST_DIR}/H5DatasetDevice.h
            ${CMAKE_CURRENT_LIST_DIR}/H5Coro.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (FATAL_ERROR "Unable to include h5 package... required libraries not found")

endif ()
