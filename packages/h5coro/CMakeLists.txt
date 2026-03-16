find_package (ZLIB)

message (STATUS "Including h5coro package")

target_compile_definitions (slideruleLib PUBLIC __h5coro__)

target_link_libraries (slideruleLib PUBLIC ${ZLIB_LIBRARIES})

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/h5coro.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/H5Column.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/H5CoroLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/H5Dense.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/H5DArray.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/H5DataFrame.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/H5Dataset.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/H5DatasetDevice.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/H5File.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/H5Object.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/H5VarSet.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/h5coro.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5Array.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5Column.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5CoroLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5Dense.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5DArray.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5DataFrame.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5Dataset.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5DatasetDevice.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5Element.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5File.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5Object.h
        ${CMAKE_CURRENT_LIST_DIR}/package/H5VarSet.h
    DESTINATION
        ${INCDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/h5.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/h5p.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/h5x.lua
    DESTINATION
        ${CONFDIR}/api
)
