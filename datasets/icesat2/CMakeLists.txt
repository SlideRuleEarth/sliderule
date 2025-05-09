message (STATUS "Including icesat2 dataset")

target_compile_definitions (slideruleLib PUBLIC __icesat2__)

target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/icesat2.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl03DataFrame.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl03Reader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl03Viewer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl03Indexer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl06Dispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl06Reader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl08Dispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl13IODriver.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl13Reader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl24DataFrame.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl24IODriver.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CumulusIODriver.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GTDArray.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Icesat2Fields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/MeritRaster.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/PhoReal.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/SurfaceFitter.cpp
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/package/UT_Atl06Dispatch.cpp>
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/package
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/icesat2.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl03DataFrame.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl03Reader.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl03Viewer.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl03Indexer.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl06Dispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl06Reader.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl08Dispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl13IODriver.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl13Reader.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl24DataFrame.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Atl24IODriver.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CumulusIODriver.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GTArray.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GTDArray.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Icesat2Fields.h
        ${CMAKE_CURRENT_LIST_DIR}/package/MeritRaster.h
        ${CMAKE_CURRENT_LIST_DIR}/package/PhoReal.h
        ${CMAKE_CURRENT_LIST_DIR}/package/SurfaceFitter.h
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/package/UT_Atl06Dispatch.h>
    DESTINATION
        ${INCDIR}/icesat2
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl03s.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl03sp.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl03v.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl03vp.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl03x.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl06.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl06p.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl06s.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl06sp.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl08.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl08p.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl13s.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl13sp.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl24x.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/indexer.lua
    DESTINATION
        ${CONFDIR}/api
)
