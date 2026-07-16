message (STATUS "Including legacy package")

target_compile_definitions (slideruleLib PUBLIC __legacy__)

target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/legacy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsFileWriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsFrameStripper.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsMsgProcessor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPacketProcessor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPublisherProcessorModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsRecordFileWriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CfsInterface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CommandableObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CommandProcessor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CosmosInterface.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaInterpreter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaLibraryCmd.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/legacy.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsFileWriter.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsFrameStripper.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsMsgProcessor.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPacketProcessor.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPublisherProcessorModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsRecordFileWriter.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CfsInterface.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CommandableObject.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CommandProcessor.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CosmosInterface.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaInterpreter.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaLibraryCmd.h
        ${CMAKE_CURRENT_LIST_DIR}/package/StatisticRecord.h
    DESTINATION
        ${INCDIR}
)
