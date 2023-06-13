# Find cURL Library
find_package (CURL)
find_package (OpenSSL REQUIRED)

# Build package
if (CURL_FOUND AND OPENSSL_FOUND)

    message (STATUS "Including aws package")

    target_compile_definitions (slideruleLib PUBLIC __aws__)

    target_link_libraries (slideruleLib PUBLIC ${CURL_LIBRARIES})
    target_link_libraries (slideruleLib PUBLIC OpenSSL::SSL)
    target_link_libraries (slideruleLib PUBLIC OpenSSL::Crypto)

    target_include_directories (slideruleLib PUBLIC ${CURL_INCLUDE_DIR})

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/aws.cpp
            ${CMAKE_CURRENT_LIST_DIR}/CredentialStore.cpp
            ${CMAKE_CURRENT_LIST_DIR}/S3CacheIODriver.cpp
            ${CMAKE_CURRENT_LIST_DIR}/S3CurlIODriver.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/aws.h
            ${CMAKE_CURRENT_LIST_DIR}/CredentialStore.h
            ${CMAKE_CURRENT_LIST_DIR}/S3CacheIODriver.h
            ${CMAKE_CURRENT_LIST_DIR}/S3CurlIODriver.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (FATAL_ERROR "Unable to include aws package... required libraries not found")

endif ()
