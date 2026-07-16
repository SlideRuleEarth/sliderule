find_package (ZLIB)
find_library (HDF5_LIB hdf5)

message (STATUS "Including hdf package")

target_compile_definitions (slideruleLib PUBLIC __hdf__)
target_compile_definitions (slideruleLib PUBLIC H5_USE_110_API)
target_compile_definitions (slideruleLib PUBLIC H5_USE_HDF5LIB)

target_link_libraries (slideruleLib PUBLIC ${ZLIB_LIBRARIES})
target_link_libraries (slideruleLib PUBLIC ${HDF5_LIB})

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/hdf.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/HdfLib.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/hdf.h
        ${CMAKE_CURRENT_LIST_DIR}/package/HdfLib.h
    DESTINATION
        ${INCDIR}
)
