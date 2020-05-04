
message (STATUS "Including sigview plugin")

add_library (sigviewLib SHARED "")

set_target_properties (sigviewLib PROPERTIES OUTPUT_NAME sigview)
set_target_properties (sigviewLib PROPERTIES PREFIX "")
set_target_properties (sigviewLib PROPERTIES POSITION_INDEPENDENT_CODE 1) # shared libraries need PIC

target_link_libraries (sigviewLib PUBLIC sliderule)

target_sources(sigviewLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/sigview.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AdasSocketReader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ItosRecord.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ItosRecordParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/DatasrvSocketReader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AtlasFileWriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AtlasHistogram.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AltimetryHistogram.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TimeTagHistogram.cpp
        ${CMAKE_CURRENT_LIST_DIR}/BceHistogram.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TimeTagProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AltimetryProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/MajorFrameProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TimeProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LaserProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/BceProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CmdEchoProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/DiagLogProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ReportProcessorStatistic.cpp
        ${CMAKE_CURRENT_LIST_DIR}/HstvsSimulator.cpp
        ${CMAKE_CURRENT_LIST_DIR}/BlinkProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/DSP.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TxTimeProcessor.cpp
)

target_include_directories (sigviewLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/sigview.h
        ${CMAKE_CURRENT_LIST_DIR}/atlasdefines.h
        ${CMAKE_CURRENT_LIST_DIR}/AdasSocketReader.h
        ${CMAKE_CURRENT_LIST_DIR}/ItosRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/ItosRecordParser.h
        ${CMAKE_CURRENT_LIST_DIR}/DatasrvSocketReader.h
        ${CMAKE_CURRENT_LIST_DIR}/AtlasFileWriter.h
        ${CMAKE_CURRENT_LIST_DIR}/AtlasHistogram.h
        ${CMAKE_CURRENT_LIST_DIR}/AltimetryHistogram.h
        ${CMAKE_CURRENT_LIST_DIR}/TimeTagHistogram.h
        ${CMAKE_CURRENT_LIST_DIR}/BceHistogram.h
        ${CMAKE_CURRENT_LIST_DIR}/TimeTagProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/AltimetryProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/MajorFrameProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/TimeProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/LaserProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/BceProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/CmdEchoProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/DiagLogProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/ReportProcessorStatistic.h
        ${CMAKE_CURRENT_LIST_DIR}/HstvsSimulator.h
        ${CMAKE_CURRENT_LIST_DIR}/BlinkProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/DSP.h
        ${CMAKE_CURRENT_LIST_DIR}/TxTimeProcessor.h
    DESTINATION
        ${INCDIR}
)

install (TARGETS sigviewLib LIBRARY DESTINATION ${CONFDIR})
