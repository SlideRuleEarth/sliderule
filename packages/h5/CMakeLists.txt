
# Find hdf5 library - prefer static over shared
list ( REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES   ".a" )
list ( INSERT      CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a" )

# Find standard HDF5 libraries
find_library (HDF5_LIB hdf5)
find_package (ZLIB)

# Find REST VOL Plugin Libraries
if(${ENABLE_H5_REST_VOL})
    find_library (CURL_LIB curl)
    find_library (YAJL_LIB yajl)
    find_library (REST_VOL_LIB  hdf5_vol_rest)
endif()

# Build package
if (HDF5_LIB AND ZLIB_FOUND)

    message (STATUS "Including h5 package")

    target_compile_definitions (sliderule PUBLIC __h5__)
    target_compile_definitions (sliderule PUBLIC H5_USE_110_API)

    if(${ENABLE_H5_REST_VOL})
        target_compile_definitions (sliderule PUBLIC H5_USE_REST_VOL)
        target_link_libraries (sliderule PUBLIC ${REST_VOL_LIB})
        target_link_libraries (sliderule PUBLIC ${CURL_LIB})
        target_link_libraries (sliderule PUBLIC ${YAJL_LIB})
    endif()

    target_link_libraries (sliderule PUBLIC ${HDF5_LIB})
    target_link_libraries (sliderule PUBLIC ${ZLIB_LIBRARIES})

    target_sources(sliderule
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/h5.cpp
            ${CMAKE_CURRENT_LIST_DIR}/H5File.cpp
            ${CMAKE_CURRENT_LIST_DIR}/H5DatasetDevice.cpp
            ${CMAKE_CURRENT_LIST_DIR}/H5Lib.cpp
    )

    target_include_directories (sliderule
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/h5.h
            ${CMAKE_CURRENT_LIST_DIR}/H5File.h
            ${CMAKE_CURRENT_LIST_DIR}/H5DatasetDevice.h
            ${CMAKE_CURRENT_LIST_DIR}/H5Lib.h
            ${CMAKE_CURRENT_LIST_DIR}/H5Array.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (STATUS "Unable to include h5 package... required libraries not found")

endif ()
