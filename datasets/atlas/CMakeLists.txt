message (STATUS "Including atlas dataset")

target_compile_definitions (slideruleLib PUBLIC __atlas__)

target_sources(slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/atlas.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ItosRecord.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ItosRecordParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/AtlasFileWriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/AtlasHistogram.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/AltimetryHistogram.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/TimeTagHistogram.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/TimeTagProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/AltimetryProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/MajorFrameProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/TimeProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LaserProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CmdEchoProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/DiagLogProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/HstvsSimulator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/SafeString.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/atlas.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ItosRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ItosRecordParser.h
        ${CMAKE_CURRENT_LIST_DIR}/package/AtlasFileWriter.h
        ${CMAKE_CURRENT_LIST_DIR}/package/AtlasHistogram.h
        ${CMAKE_CURRENT_LIST_DIR}/package/AltimetryHistogram.h
        ${CMAKE_CURRENT_LIST_DIR}/package/TimeTagHistogram.h
        ${CMAKE_CURRENT_LIST_DIR}/package/TimeTagProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/AltimetryProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/MajorFrameProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/TimeProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LaserProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CmdEchoProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/DiagLogProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/HstvsSimulator.h
        ${CMAKE_CURRENT_LIST_DIR}/package/SafeString.h
    DESTINATION
        ${INCDIR}/atlas
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl00.lua
    DESTINATION
        ${CONFDIR}/api
)

