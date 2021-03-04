find_library (MONGOOSE_LIB mongoose)

# Build plugin
if (MONGOOSE_LIB)

    message (STATUS "Including mongoose package")

    target_compile_definitions (slideruleLib PUBLIC __mongoose__)

    target_link_libraries (slideruleLib PUBLIC ${MONGOOSE_LIB})

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/mongoose.cpp
            ${CMAKE_CURRENT_LIST_DIR}/MongooseServer.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/mongoose.h
            ${CMAKE_CURRENT_LIST_DIR}/MongooseServer.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (STATUS "Unable to include mongoose package... library not found")

endif ()
