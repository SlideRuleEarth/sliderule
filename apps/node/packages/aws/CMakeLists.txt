find_package (CURL)
find_package (OpenSSL REQUIRED)
find_package(AWSSDK REQUIRED COMPONENTS s3 firehose secretsmanager)

message (STATUS "Including aws package")

target_compile_definitions (slideruleLib PUBLIC __aws__)

target_link_libraries (slideruleLib PUBLIC ${CURL_LIBRARIES})
target_link_libraries (slideruleLib PUBLIC OpenSSL::SSL)
target_link_libraries (slideruleLib PUBLIC OpenSSL::Crypto)
target_link_libraries (slideruleLib PUBLIC ${AWSSDK_LINK_LIBRARIES})

target_include_directories (slideruleLib PUBLIC ${CURL_INCLUDE_DIR})

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/aws.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/FirehoseMonitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/SecretManager.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/S3IODriver.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/aws.h
        ${CMAKE_CURRENT_LIST_DIR}/package/FirehoseMonitor.h
        ${CMAKE_CURRENT_LIST_DIR}/package/SecretManager.h
        ${CMAKE_CURRENT_LIST_DIR}/package/S3IODriver.h
    DESTINATION
        ${INCDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/extensions/aws_utils.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/iam_role_auth.lua
    DESTINATION
        ${CONFDIR}/ext
)