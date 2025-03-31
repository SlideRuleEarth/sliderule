message (STATUS "Including bathy dataset")

target_compile_definitions (slideruleLib PUBLIC __bathy__)

target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/bathy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyDataFrame.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyGranule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyKd.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyMask.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyRefractionCorrector.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathySeaSurfaceFinder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathySignalStrength.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyUncertaintyCalculator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyViewer.cpp
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/package/UT_BathyRefractionCorrector.cpp>
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/bathy.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyDataFrame.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyFields.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyGranule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyKd.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyMask.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyRefractionCorrector.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathySeaSurfaceFinder.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathySignalStrength.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyUncertaintyCalculator.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyViewer.h
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/package/UT_BathyRefractionCorrector.h>
    DESTINATION
        ${INCDIR}/bathy
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/bathy.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/bathyv.lua
    DESTINATION
        ${CONFDIR}/api
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/extensions/bathy_init.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/bathy_utils.lua
    DESTINATION
        ${CONFDIR}
)
