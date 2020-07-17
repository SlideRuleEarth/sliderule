message (STATUS "Including icesat2 plugin")

add_library (icesat2Lib SHARED "")

set_target_properties (icesat2Lib PROPERTIES OUTPUT_NAME icesat2)
set_target_properties (icesat2Lib PROPERTIES PREFIX "")
set_target_properties (icesat2Lib PROPERTIES POSITION_INDEPENDENT_CODE 1) # shared libraries need PIC

target_link_libraries (icesat2Lib PUBLIC sliderule)
target_include_directories (icesat2Lib PUBLIC ${CMAKE_SOURCE_DIR}/plugins/h5)

target_sources(icesat2Lib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/icesat2.cpp
        ${CMAKE_CURRENT_LIST_DIR}/lua_parms.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Atl03Device.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Atl06Dispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UT_Atl06Dispatch.cpp
)

target_include_directories (icesat2Lib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/icesat2.h
        ${CMAKE_CURRENT_LIST_DIR}/lua_parms.h
        ${CMAKE_CURRENT_LIST_DIR}/Atl03Device.h
        ${CMAKE_CURRENT_LIST_DIR}/Atl06Dispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/GTArray.h
        ${CMAKE_CURRENT_LIST_DIR}/UT_Atl06Dispatch.h
    DESTINATION
        ${INCDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl06.lua
        ${CMAKE_CURRENT_LIST_DIR}/config/asset_directory.csv
    DESTINATION
        ${CONFDIR}
)

install (TARGETS icesat2Lib LIBRARY DESTINATION ${CONFDIR})
