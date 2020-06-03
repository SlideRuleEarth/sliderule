# Find zlib library
find_package(ZLIB)

# Find hdf5 library - prefer static over shared
list( REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES   ".a" )
list( INSERT      CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a" )
find_library (HDF5_LIB hdf5)

# Build plugin
if (HDF5_LIB AND ZLIB_FOUND)

    message (STATUS "Including icesat2 plugin")

    add_library (icesat2Lib SHARED "")

    set_target_properties (icesat2Lib PROPERTIES OUTPUT_NAME icesat2)
    set_target_properties (icesat2Lib PROPERTIES PREFIX "")
    set_target_properties (icesat2Lib PROPERTIES POSITION_INDEPENDENT_CODE 1) # shared libraries need PIC

    target_link_libraries (icesat2Lib PUBLIC sliderule)
    target_link_libraries (icesat2Lib PUBLIC ${HDF5_LIB})
    target_link_libraries (icesat2Lib PUBLIC ${ZLIB_LIBRARIES})

    target_compile_definitions (icesat2Lib PUBLIC H5_USE_110_API)

    target_sources(icesat2Lib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/icesat2.cpp
            ${CMAKE_CURRENT_LIST_DIR}/Hdf5File.cpp
            ${CMAKE_CURRENT_LIST_DIR}/Hdf5Handle.cpp
    )

    target_include_directories (icesat2Lib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/icesat2.h
            ${CMAKE_CURRENT_LIST_DIR}/Hdf5File.h
            ${CMAKE_CURRENT_LIST_DIR}/Hdf5Handle.h
        DESTINATION
            ${INCDIR}
    )

    install (TARGETS icesat2Lib LIBRARY DESTINATION ${CONFDIR})

else ()

    message (STATUS "Unable to include icesat2 plugin... required libraries not found")

endif ()
