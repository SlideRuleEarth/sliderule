
# Prefer static over shared
list ( REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES   ".a" )
list ( INSERT      CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a" )

# Find AWS libraries
find_package(AWSSDK REQUIRED COMPONENTS s3 transfer)

# Build package
if (AWSSDK_LINK_LIBRARIES)

    message (STATUS "Including aws package")

    target_compile_definitions (sliderule PUBLIC __aws__)

    #target_compile_options (sliderule PUBLIC "$<$<CONFIG:Debug>:-DENABLE_AWS_LOGGING>")

    target_link_libraries (sliderule PUBLIC ${AWSSDK_LINK_LIBRARIES})

    target_sources(sliderule
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/aws.cpp
            ${CMAKE_CURRENT_LIST_DIR}/S3Lib.cpp
    )

    target_include_directories (sliderule
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/aws.h
            ${CMAKE_CURRENT_LIST_DIR}/S3Lib.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (STATUS "Unable to include aws package... required libraries not found")

endif ()
