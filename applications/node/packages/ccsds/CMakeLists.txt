message (STATUS "Including ccsds package")

target_compile_definitions (slideruleLib PUBLIC __ccsds__)

target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/ccsds.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPacket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPacketizer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPacketInterleaver.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPacketParser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsParserAOSFrameModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsParserModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsParserStripModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsParserZFrameModule.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPayloadDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsRecord.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsRecordDispatcher.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/ccsds.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPacket.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPacketizer.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPacketInterleaver.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPacketParser.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsParserAOSFrameModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsParserModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsParserStripModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsParserZFrameModule.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsPayloadDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CcsdsRecordDispatcher.h
    DESTINATION
        ${INCDIR}
)
