# Find pybind11 Library
find_package(pybind11 REQUIRED)

# Build modeul
if (pybind11_FOUND)

    message (STATUS "Building SRPYBIN Python bindings")

    pybind11_add_module(srpybin
        ${CMAKE_CURRENT_LIST_DIR}/pyH5Coro.cpp
        ${CMAKE_CURRENT_LIST_DIR}/pyLua.cpp
        ${CMAKE_CURRENT_LIST_DIR}/pyLogger.cpp
        ${CMAKE_CURRENT_LIST_DIR}/pyPlugin.cpp
        ${CMAKE_CURRENT_LIST_DIR}/pyCredentialStore.cpp
        ${CMAKE_CURRENT_LIST_DIR}/pyS3Cache.cpp
        ${CMAKE_CURRENT_LIST_DIR}/init.cpp)

    target_include_directories (srpybin PRIVATE ${CMAKE_CURRENT_LIST_DIR})

    target_link_libraries (srpybin PUBLIC -Wl,--whole-archive  slideruleLib  -Wl,--no-whole-archive)

    set_target_properties (srpybin PROPERTIES PREFIX "")
    set_target_properties (srpybin PROPERTIES CXX_STANDARD ${CXX_VERSION})

    if( ICESAT2_PLUGIN_LIBPATH )
        message (STATUS "Including icesat2 plugin in python bindings")
        add_library(icesat2_plugin UNKNOWN IMPORTED)
        set_property(TARGET icesat2_plugin PROPERTY IMPORTED_LOCATION "${ICESAT2_PLUGIN_LIBPATH}")
        target_link_libraries (srpybin PUBLIC icesat2_plugin)
        target_include_directories (srpybin PRIVATE ${ICESAT2_PLUGIN_INCPATH})
        target_compile_definitions (srpybin PUBLIC __icesat2__)
    endif()

    install (TARGETS srpybin DESTINATION ${INSTALLDIR}/lib)

else ()

    message (FATAL_ERROR "Unable to build SRPYBIN bindings... required libraries not found")

endif ()
