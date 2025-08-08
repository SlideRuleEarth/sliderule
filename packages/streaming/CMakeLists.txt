message (STATUS "Including streaming package")

target_compile_definitions (slideruleLib PUBLIC __streaming__)

target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/streaming.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/AncillaryFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CaptureDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ClusterSocket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ContainerRecord.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CsvDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/DeviceIO.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/DeviceObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/DeviceReader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/DeviceWriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/DispatchObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/File.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/HttpClient.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LimitDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LimitRecord.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/MetricDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/MetricRecord.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/MsgBridge.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/MsgProcessor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/PublisherDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/RecordDispatcher.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ReportDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/TcpSocket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Uart.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/UdpSocket.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/streaming.h
        ${CMAKE_CURRENT_LIST_DIR}/package/AncillaryFields.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CaptureDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ClusterSocket.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ContainerRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CsvDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/package/DeviceIO.h
        ${CMAKE_CURRENT_LIST_DIR}/package/DeviceObject.h
        ${CMAKE_CURRENT_LIST_DIR}/package/DeviceReader.h
        ${CMAKE_CURRENT_LIST_DIR}/package/DeviceWriter.h
        ${CMAKE_CURRENT_LIST_DIR}/package/DispatchObject.h
        ${CMAKE_CURRENT_LIST_DIR}/package/File.h
        ${CMAKE_CURRENT_LIST_DIR}/package/HttpClient.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LimitDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LimitRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/package/MetricDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/package/MetricRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/package/MsgBridge.h
        ${CMAKE_CURRENT_LIST_DIR}/package/MsgProcessor.h
        ${CMAKE_CURRENT_LIST_DIR}/package/PublisherDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/package/RecordDispatcher.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ReportDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/package/TcpSocket.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Uart.h
        ${CMAKE_CURRENT_LIST_DIR}/package/UdpSocket.h
    DESTINATION
        ${INCDIR}
)

# Endpoints #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/example_engine_endpoint.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/example_source_endpoint.lua
    DESTINATION
        ${CONFDIR}/api
)

# Extensions #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/extensions/georesource.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/proxy.lua
    DESTINATION
        ${CONFDIR}
)
