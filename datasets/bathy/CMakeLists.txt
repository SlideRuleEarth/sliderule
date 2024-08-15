message (STATUS "Including bathy dataset")

target_compile_definitions (slideruleLib PUBLIC __bathy__)

target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/bathy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyClassifier.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyParms.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyReader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyViewer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyRefractionCorrector.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyUncertaintyCalculator.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/bathy.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyClassifier.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyParms.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyReader.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyViewer.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyRefractionCorrector.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyUncertaintyCalculator.h
    DESTINATION
        ${INCDIR}/bathy
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl24g.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl24v.lua
    DESTINATION
        ${CONFDIR}/api
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/extensions/atl24_init.lua
    DESTINATION
        ${CONFDIR}
)
