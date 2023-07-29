# Find Arrow Library
find_package(Arrow)
find_package(Parquet REQUIRED)

# Build package
if (Arrow_FOUND AND Parquet_FOUND)

    message (STATUS "Including arrow package")

    target_compile_definitions (slideruleLib PUBLIC __arrow__)

    target_link_libraries(slideruleLib PUBLIC Arrow::arrow_shared)
    target_link_libraries(slideruleLib PUBLIC Parquet::parquet_shared)

    target_sources(slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/arrow.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ArrowParms.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ParquetBuilder.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ParquetWriter.cpp
    )

    target_include_directories (slideruleLib
        PUBLIC
            $<INSTALL_INTERFACE:${INCDIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/arrow.h
            ${CMAKE_CURRENT_LIST_DIR}/ArrowParms.h
            ${CMAKE_CURRENT_LIST_DIR}/ParquetBuilder.h
            ${CMAKE_CURRENT_LIST_DIR}/ParquetWriter.h
        DESTINATION
            ${INCDIR}
    )

else ()

    message (FATAL_ERROR "Unable to include arrow package... required libraries not found")

endif ()
