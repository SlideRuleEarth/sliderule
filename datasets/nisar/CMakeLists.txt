message (STATUS "Including nisar dataset")

target_compile_definitions (slideruleLib PUBLIC __nisar__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/nisar.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/NisarDataset.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/nisar.h
        ${CMAKE_CURRENT_LIST_DIR}/package/NisarDataset.h
    DESTINATION
        ${INCDIR}/nisar
)
