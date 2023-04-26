# Find cURL Library
find_package (CURL)

# Find RapidJSON Library
find_package (RapidJSON)

# Build package
if (CURL_FOUND AND RapidJSON_FOUND)

    message (STATUS "Including netsvc package")

    target_compile_definitions (slideruleLib PUBLIC __netsvc__)

    target_link_libraries (slideruleLib PUBLIC ${CURL_LIBRARIES})
    target_include_directories (slideruleLib PUBLIC ${CURL_INCLUDE_DIR})
    target_include_directories (slideruleLib PUBLIC ${RapidJSON_INCLUDE_DIR})

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/netsvc.cpp
            ${CMAKE_CURRENT_LIST_DIR}/CurlLib.cpp
            ${CMAKE_CURRENT_LIST_DIR}/EndpointProxy.cpp
            ${CMAKE_CURRENT_LIST_DIR}/OrchestratorLib.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ProvisioningSystemLib.cpp
            ${CMAKE_CURRENT_LIST_DIR}/NetsvcParms.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/netsvc.h
            ${CMAKE_CURRENT_LIST_DIR}/CurlLib.h
            ${CMAKE_CURRENT_LIST_DIR}/EndpointProxy.h
            ${CMAKE_CURRENT_LIST_DIR}/OrchestratorLib.h
            ${CMAKE_CURRENT_LIST_DIR}/ProvisioningSystemLib.h
            ${CMAKE_CURRENT_LIST_DIR}/NetsvcParms.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (FATAL_ERROR "Unable to include netsvc package... required libraries not found")

endif ()
