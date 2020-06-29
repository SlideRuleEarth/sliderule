# Find zlib library
find_package(ZLIB)

# Find hdf5 library - prefer static over shared
list( REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES   ".a" )
list( INSERT      CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a" )
find_library (HDF5_LIB hdf5)

# Build package
if (HDF5_LIB AND ZLIB_FOUND)

    message (STATUS "Including h5 package")

    target_compile_definitions (sliderule PUBLIC __h5__)
    target_compile_definitions (sliderule PUBLIC H5_USE_110_API)

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
