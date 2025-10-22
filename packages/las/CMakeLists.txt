find_package(PDAL REQUIRED)

if(NOT TARGET PDAL::pdalcpp)
    add_library(PDAL::pdalcpp ALIAS pdalcpp)
endif()

message(STATUS "Including las package")

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/las.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LasDataFrame.cpp
)

target_compile_definitions(slideruleLib PUBLIC __las__)

target_include_directories(slideruleLib
    PUBLIC
        ${PDAL_INCLUDE_DIRS}
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

target_link_libraries(slideruleLib PUBLIC PDAL::pdalcpp)

install(
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/las.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LasDataFrame.h
    DESTINATION
        ${INCDIR}
)