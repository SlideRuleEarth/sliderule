
message (STATUS "Including atlas plugin")

add_library (atlasLib SHARED "")
set_property(GLOBAL APPEND PROPERTY plugins "atlas\n")

set_target_properties (atlasLib PROPERTIES OUTPUT_NAME atlas)
set_target_properties (atlasLib PROPERTIES PREFIX "")
set_target_properties (atlasLib PROPERTIES POSITION_INDEPENDENT_CODE 1) # shared libraries need PIC

target_link_libraries (atlasLib PUBLIC sliderule)

target_sources(atlasLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/atlas.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ItosRecord.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ItosRecordParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AtlasFileWriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AtlasHistogram.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AltimetryHistogram.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TimeTagHistogram.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TimeTagProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AltimetryProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/MajorFrameProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TimeProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LaserProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CmdEchoProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/DiagLogProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/HstvsSimulator.cpp
)

target_include_directories (atlasLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/atlas.h
        ${CMAKE_CURRENT_LIST_DIR}/atlasdefines.h
        ${CMAKE_CURRENT_LIST_DIR}/ItosRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/ItosRecordParser.h
        ${CMAKE_CURRENT_LIST_DIR}/AtlasFileWriter.h
        ${CMAKE_CURRENT_LIST_DIR}/AtlasHistogram.h
        ${CMAKE_CURRENT_LIST_DIR}/AltimetryHistogram.h
        ${CMAKE_CURRENT_LIST_DIR}/TimeTagHistogram.h
        ${CMAKE_CURRENT_LIST_DIR}/TimeTagProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/AltimetryProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/MajorFrameProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/TimeProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/LaserProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/CmdEchoProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/DiagLogProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/HstvsSimulator.h
    DESTINATION
        ${INCDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/atl00.lua
    DESTINATION
        ${CONFDIR}
)

install (TARGETS atlasLib LIBRARY DESTINATION ${CONFDIR})
