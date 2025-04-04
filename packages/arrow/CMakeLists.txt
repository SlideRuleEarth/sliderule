find_package(Arrow)
find_package(Parquet REQUIRED)

message (STATUS "Including arrow package")

target_compile_definitions (slideruleLib PUBLIC __arrow__)

target_link_libraries(slideruleLib PUBLIC Arrow::arrow_shared)
target_link_libraries(slideruleLib PUBLIC Parquet::parquet_shared)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/arrow.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowBuilder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowBuilderImpl.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowDataFrame.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowEndpoint.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowSampler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowSamplerImpl.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/arrow.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowBuilder.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowBuilderImpl.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowDataFrame.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowEndpoint.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowFields.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowSampler.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ArrowSamplerImpl.h
    DESTINATION
        ${INCDIR}
)