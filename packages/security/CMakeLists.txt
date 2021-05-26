# Find OpenSSL ssl Library
find_package (OpenSSL)

# Build package
if (OPENSSL_FOUND)

    message (STATUS "Including security package")

    target_compile_definitions (slideruleLib PUBLIC __security__)

    target_link_libraries (slideruleLib PUBLIC ${OPENSSL_SSL_LIBRARY})
    target_include_directories (slideruleLib PUBLIC ${OPENSSL_INCLUDE_DIR})

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/security.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/security.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (FATAL_ERROR "Unable to include security package... required libraries not found")

endif ()
