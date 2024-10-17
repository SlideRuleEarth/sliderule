# Find cURL Library
find_package (CURL)

# Find RapidJSON Library
find_package (RapidJSON)

# Build package
if (CURL_FOUND AND RapidJSON_FOUND)

    message (STATUS "Including cre package")

    target_compile_definitions (slideruleLib PUBLIC __cre__)

    target_link_libraries (slideruleLib PUBLIC ${CURL_LIBRARIES})
    target_include_directories (slideruleLib PUBLIC ${CURL_INCLUDE_DIR})
    target_include_directories (slideruleLib PUBLIC ${RapidJSON_INCLUDE_DIR})

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/cre.cpp
            ${CMAKE_CURRENT_LIST_DIR}/CreFields.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ContainerRunner.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/cre.h
            ${CMAKE_CURRENT_LIST_DIR}/CreFields.h
            ${CMAKE_CURRENT_LIST_DIR}/ContainerRunner.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (FATAL_ERROR "Unable to include cre package... required libraries not found")

endif ()
