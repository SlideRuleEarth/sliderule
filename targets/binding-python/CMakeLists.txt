# Find pybind11 Library
find_package(pybind11 REQUIRED)

# Build Bindings
if (pybind11_FOUND)

    # PyBind11 Module
    message (STATUS "Building srpybin Python bindings")

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

    install (TARGETS srpybin DESTINATION ${INSTALLDIR}/lib)

    # ICESat-2 Plugin #
    message (STATUS "Including icesat2 plugin in python bindings")
    target_compile_definitions (slideruleLib PUBLIC __icesat2__)
    target_compile_definitions (slideruleLib PUBLIC BINID=LIBID)
    target_sources(slideruleLib
        PRIVATE
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/icesat2.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/Atl03Reader.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/Atl03Indexer.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/Atl06Dispatch.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/Atl08Dispatch.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/CumulusIODriver.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/GTDArray.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/Icesat2Parms.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/MeritRaster.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/PluginMetrics.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/UT_Atl03Reader.cpp
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin/UT_Atl06Dispatch.cpp
    )
    target_include_directories (slideruleLib
        PUBLIC
            ${PROJECT_SOURCE_DIR}/plugins/icesat2/plugin
            $<INSTALL_INTERFACE:${INCDIR}>
    )

else ()

    message (FATAL_ERROR "Unable to build srpybin bindings... required libraries not found")

endif ()
