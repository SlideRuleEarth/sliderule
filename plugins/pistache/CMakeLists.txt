# Find pistache library - prefer static over shared
list( REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES   ".a" )
list( INSERT      CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a" )
find_library (PISTACHE_LIB pistache)

# Build plugin
if (PISTACHE_LIB)

    message (STATUS "Including pistache plugin")

    add_library (pistacheLib SHARED "")

    set_target_properties (pistacheLib PROPERTIES OUTPUT_NAME pistache)
    set_target_properties (pistacheLib PROPERTIES PREFIX "")
    set_target_properties (pistacheLib PROPERTIES POSITION_INDEPENDENT_CODE 1) # shared libraries need PIC

    target_link_libraries (pistacheLib PUBLIC sliderule)
    target_link_libraries (pistacheLib PUBLIC ${PISTACHE_LIB})
    target_link_libraries (pistacheLib PUBLIC ssl)

    target_sources(pistacheLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/pistache.cpp
            ${CMAKE_CURRENT_LIST_DIR}/LuaEndpoint.cpp
            ${CMAKE_CURRENT_LIST_DIR}/LuaClient.cpp
            ${CMAKE_CURRENT_LIST_DIR}/RouteHandler.cpp
    )

    target_include_directories (pistacheLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/pistache.h
            ${CMAKE_CURRENT_LIST_DIR}/LuaEndpoint.h
            ${CMAKE_CURRENT_LIST_DIR}/LuaClient.h
            ${CMAKE_CURRENT_LIST_DIR}/RouteHandler.h
        DESTINATION
            ${INCDIR}
    )

    install (TARGETS pistacheLib LIBRARY DESTINATION ${CONFDIR})

else ()

    message (STATUS "Unable to include pistache plugin... library not found")

endif ()
