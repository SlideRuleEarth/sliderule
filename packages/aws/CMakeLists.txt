find_package (CURL)
find_package (OpenSSL REQUIRED)

target_compile_definitions (slideruleLib PUBLIC __aws__)

target_link_libraries (slideruleLib PUBLIC ${CURL_LIBRARIES})
target_link_libraries (slideruleLib PUBLIC OpenSSL::SSL)
target_link_libraries (slideruleLib PUBLIC OpenSSL::Crypto)

target_include_directories (slideruleLib PUBLIC ${CURL_INCLUDE_DIR})

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/aws.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CredentialStore.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/S3CacheIODriver.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/S3CurlIODriver.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/aws.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CredentialStore.h
        ${CMAKE_CURRENT_LIST_DIR}/package/S3CacheIODriver.h
        ${CMAKE_CURRENT_LIST_DIR}/package/S3CurlIODriver.h
    DESTINATION
        ${INCDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/extensions/aws_utils.lua
    DESTINATION
        ${CONFDIR}
)