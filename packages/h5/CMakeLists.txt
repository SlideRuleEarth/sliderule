find_package (ZLIB)

# Find standard HDF5 libraries
find_library (HDF5_LIB hdf5)

# Find REST VOL Plugin Libraries
find_library (CURL_LIB curl)
find_library (YAJL_LIB yajl)
find_library (REST_VOL_LIB  hdf5_vol_rest)

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
            ${CMAKE_CURRENT_LIST_DIR}/H5Lite.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/h5.h
            ${CMAKE_CURRENT_LIST_DIR}/H5Api.h
            ${CMAKE_CURRENT_LIST_DIR}/H5Array.h
            ${CMAKE_CURRENT_LIST_DIR}/H5File.h
            ${CMAKE_CURRENT_LIST_DIR}/H5DatasetDevice.h
            ${CMAKE_CURRENT_LIST_DIR}/H5Lite.h
        DESTINATION
            ${INCDIR}
    )

    if (HDF5_LIB AND ENABLE_HDF5_LIB)

        message (STATUS "Using hdf5 library")

        target_compile_definitions (slideruleLib PUBLIC H5_USE_110_API)
        target_compile_definitions (slideruleLib PUBLIC H5_USE_HDF5LIB)

        if(${ENABLE_H5_REST_VOL})
            target_compile_definitions (slideruleLib PUBLIC H5_USE_REST_VOL)
            target_link_libraries (slideruleLib PUBLIC ${REST_VOL_LIB})
            target_link_libraries (slideruleLib PUBLIC ${CURL_LIB})
            target_link_libraries (slideruleLib PUBLIC ${YAJL_LIB})
        endif()

        target_link_libraries (slideruleLib PUBLIC ${HDF5_LIB})

        target_sources (slideruleLib PRIVATE ${CMAKE_CURRENT_LIST_DIR}/H5Lib.cpp)
        
        install (FILES ${CMAKE_CURRENT_LIST_DIR}/H5Lib.h DESTINATION ${INCDIR})

    endif()

else ()

    message (STATUS "Unable to include h5 package... required libraries not found")

endif ()
