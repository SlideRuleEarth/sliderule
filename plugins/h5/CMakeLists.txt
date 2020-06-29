# Find zlib library
find_package(ZLIB)

# Find hdf5 library - prefer static over shared
list( REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES   ".a" )
list( INSERT      CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a" )
find_library (HDF5_LIB hdf5)

# Build plugin
if (HDF5_LIB AND ZLIB_FOUND)

    message (STATUS "Including h5 plugin")
    message (STATUS ${HDF5_LIB})
    add_library (h5Lib SHARED "")

    set_target_properties (h5Lib PROPERTIES OUTPUT_NAME h5)
    set_target_properties (h5Lib PROPERTIES PREFIX "")
    set_target_properties (h5Lib PROPERTIES POSITION_INDEPENDENT_CODE 1) # shared libraries need PIC

    target_link_libraries (h5Lib PUBLIC sliderule)
    target_link_libraries (h5Lib PUBLIC ${HDF5_LIB})
    target_link_libraries (h5Lib PUBLIC ${ZLIB_LIBRARIES})

    target_compile_definitions (h5Lib PUBLIC H5_USE_110_API)

    target_sources(h5Lib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/h5.cpp
            ${CMAKE_CURRENT_LIST_DIR}/H5File.cpp
            ${CMAKE_CURRENT_LIST_DIR}/H5DatasetDevice.cpp
            ${CMAKE_CURRENT_LIST_DIR}/H5IO.cpp
    )

    target_include_directories (h5Lib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/h5.h
            ${CMAKE_CURRENT_LIST_DIR}/H5File.h
            ${CMAKE_CURRENT_LIST_DIR}/H5DatasetDevice.h
            ${CMAKE_CURRENT_LIST_DIR}/H5IO.h
            ${CMAKE_CURRENT_LIST_DIR}/H5Array.h
        DESTINATION
            ${INCDIR}
    )

    install (TARGETS h5Lib LIBRARY DESTINATION ${CONFDIR})

else ()

    message (STATUS "Unable to include h5 plugin... required libraries not found")

endif ()
