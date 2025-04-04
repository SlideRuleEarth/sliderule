find_package (CURL)
find_package (RapidJSON)

message (STATUS "Including cre package")

target_compile_definitions (slideruleLib PUBLIC __cre__)

target_link_libraries (slideruleLib PUBLIC ${CURL_LIBRARIES})
target_include_directories (slideruleLib PUBLIC ${CURL_INCLUDE_DIR})
target_include_directories (slideruleLib PUBLIC ${RapidJSON_INCLUDE_DIR})

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/cre.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CreFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ContainerRunner.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/cre.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CreFields.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ContainerRunner.h
    DESTINATION
        ${INCDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/extensions/container_runtime.lua
    DESTINATION
        ${CONFDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/lscre.lua
    DESTINATION
        ${CONFDIR}/api
)
