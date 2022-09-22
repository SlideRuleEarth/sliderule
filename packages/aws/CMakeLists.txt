
# Prefer static over shared
list ( REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES   ".a" )
list ( INSERT      CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a" )

# Find AWS libraries
find_package(AWSSDK REQUIRED COMPONENTS s3-crt transfer)

# Build package
if (AWSSDK_LINK_LIBRARIES)

    message (STATUS "Including aws package")

    target_compile_definitions (slideruleLib PUBLIC __aws__)

    target_compile_options (slideruleLib PUBLIC "$<$<CONFIG:Debug>:-DENABLE_AWS_LOGGING>")

    target_link_libraries (slideruleLib PUBLIC ${AWSSDK_LINK_LIBRARIES})

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/aws.cpp
            ${CMAKE_CURRENT_LIST_DIR}/CredentialStore.cpp
            ${CMAKE_CURRENT_LIST_DIR}/S3IODriver.cpp
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
            ${CMAKE_CURRENT_LIST_DIR}/S3IODriver.h
            ${CMAKE_CURRENT_LIST_DIR}/S3CacheIODriver.h
            ${CMAKE_CURRENT_LIST_DIR}/S3CurlIODriver.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (FATAL_ERROR "Unable to include aws package... required libraries not found")

endif ()
