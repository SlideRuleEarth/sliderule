message (STATUS "Including legacy package")

target_compile_definitions (sliderule PUBLIC _LEGACY_)

target_sources (sliderule
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/legacy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsFileWriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsFrameStripper.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsMsgProcessor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsPacketProcessor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsPublisherProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsRecordFileWriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CfsInterface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CommandableObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CommandProcessor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CosmosInterface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LuaInterpreter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LuaLibraryCmd.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UT_Dictionary.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UT_MsgQ.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UT_TimeLib.cpp
)

target_include_directories (sliderule
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/legacy.h
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsFileWriter.h
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsFrameStripper.h
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsMsgProcessor.h
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsPacketProcessor.h
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsPublisherProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/CcsdsRecordFileWriter.h
        ${CMAKE_CURRENT_LIST_DIR}/CfsInterface.h
        ${CMAKE_CURRENT_LIST_DIR}/CommandableObject.h
        ${CMAKE_CURRENT_LIST_DIR}/CommandProcessor.h
        ${CMAKE_CURRENT_LIST_DIR}/CosmosInterface.h
        ${CMAKE_CURRENT_LIST_DIR}/LuaInterpreter.h
        ${CMAKE_CURRENT_LIST_DIR}/LuaLibraryCmd.h
        ${CMAKE_CURRENT_LIST_DIR}/StatisticRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/UT_Dictionary.h
        ${CMAKE_CURRENT_LIST_DIR}/UT_MsgQ.h
        ${CMAKE_CURRENT_LIST_DIR}/UT_TimeLib.h
    DESTINATION
        ${INCDIR}
)
