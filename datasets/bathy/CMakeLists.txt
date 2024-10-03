message (STATUS "Including bathy dataset")

find_package(OpenMP REQUIRED)
find_package(xgboost REQUIRED)

target_compile_definitions (slideruleLib PUBLIC __bathy__)

target_compile_options (slideruleLib PUBLIC ${OpenMP_CXX_FLAGS})
target_link_libraries(slideruleLib PUBLIC xgboost::xgboost)
target_link_libraries(slideruleLib PUBLIC OpenMP::OpenMP_CXX)

# CoastNet
set (COASTNETDIR ${PROJECT_SOURCE_DIR}/../ut-ATL24-coastnet CACHE STRING "Coastnet application source directory")
target_link_libraries(slideruleLib PUBLIC xgboost::xgboost)
target_compile_definitions (slideruleLib PUBLIC COASTNET_INCLUDE="${COASTNETDIR}/apps/classify_inc.h")
execute_process (COMMAND git --work-tree ${COASTNETDIR} --git-dir ${COASTNETDIR}/.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE COASTNETINFO)
string(REGEX REPLACE "\n$" "" COASTNETINFO "${COASTNETINFO}")
target_compile_definitions (slideruleLib PUBLIC COASTNETINFO="${COASTNETINFO}")

# Qtrees
set (QTREESDIR ${PROJECT_SOURCE_DIR}/../ut-ATL24-qtrees CACHE STRING "Qtrees application source directory")
target_compile_definitions (slideruleLib PUBLIC QTREES_INCLUDE="${QTREESDIR}/apps/classify_inc.h")
execute_process (COMMAND git --work-tree ${QTREESDIR} --git-dir ${QTREESDIR}/.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE QTREESINFO)
string(REGEX REPLACE "\n$" "" QTREESINFO "${QTREESINFO}")
target_compile_definitions (slideruleLib PUBLIC QTREESINFO="${QTREESINFO}")

# OpenOceansPP
set (OPENOCEANSPPDIR ${PROJECT_SOURCE_DIR}/../ut-ATL24-oopp CACHE STRING "OpenOceans++ application source directory")
target_compile_definitions (slideruleLib PUBLIC OPENOCEANSPP_INCLUDE="${OPENOCEANSPPDIR}/apps/classify_inc.h")
execute_process (COMMAND git --work-tree ${OPENOCEANSPPDIR} --git-dir ${OPENOCEANSPPDIR}/.git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE OPENOCEANSPPINFO)
string(REGEX REPLACE "\n$" "" OPENOCEANSPPINFO "${OPENOCEANSPPINFO}")
target_compile_definitions (slideruleLib PUBLIC OPENOCEANSPPINFO="${OPENOCEANSPPINFO}")

target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/bathy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyCoastnetClassifier.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyDataFrame.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyGranule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyKd.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyMask.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyOpenOceansPPClassifier.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyQtreesClassifier.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyRefractionCorrector.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathySeaSurfaceFinder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyUncertaintyCalculator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyViewer.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/bathy.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyCoastnetClassifier.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyDataFrame.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyFields.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyGranule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyKd.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyMask.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyOpenOceansPPClassifier.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyQtreesClassifier.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyRefractionCorrector.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathySeaSurfaceFinder.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyUncertaintyCalculator.h
        ${CMAKE_CURRENT_LIST_DIR}/package/BathyViewer.h
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
