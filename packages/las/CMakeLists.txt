message (STATUS "Including las package")

target_compile_definitions (slideruleLib PUBLIC __las__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/las.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LasDataFrame.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/las.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LasDataFrame.h
    DESTINATION
        ${INCDIR}
)
