# Find pistache library - prefer static over shared
list( REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES   ".a" )
list( INSERT      CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a" )
find_library (PISTACHE_LIB pistache)

# Build plugin
if (PISTACHE_LIB)

    message (STATUS "Including pistache package")

    target_compile_definitions (slideruleLib PUBLIC __pistache__)

    target_link_libraries (slideruleLib PUBLIC ${PISTACHE_LIB})
    target_link_libraries (slideruleLib PUBLIC ssl)
    target_link_libraries (slideruleLib PUBLIC crypto)

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/pistache.cpp
            ${CMAKE_CURRENT_LIST_DIR}/PistacheServer.cpp
            ${CMAKE_CURRENT_LIST_DIR}/PistacheClient.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ProgressMessager.cpp
            ${CMAKE_CURRENT_LIST_DIR}/RouteHandler.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/pistache.h
            ${CMAKE_CURRENT_LIST_DIR}/PistacheServer.h
            ${CMAKE_CURRENT_LIST_DIR}/PistacheClient.h
            ${CMAKE_CURRENT_LIST_DIR}/ProgressMessager.h
            ${CMAKE_CURRENT_LIST_DIR}/RouteHandler.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (STATUS "Unable to include pistache package... library not found")

endif ()
