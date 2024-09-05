message (STATUS "Including stream package")

target_compile_definitions (slideruleLib PUBLIC __stream__)

target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/stream.cpp
        ${CMAKE_CURRENT_LIST_DIR}/AncillaryFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CaptureDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ClusterSocket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ContainerRecord.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CsvDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/DeviceIO.cpp
        ${CMAKE_CURRENT_LIST_DIR}/DeviceObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/DeviceReader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/DeviceWriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/DispatchObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/File.cpp
        ${CMAKE_CURRENT_LIST_DIR}/HttpClient.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LimitDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LimitRecord.cpp
        ${CMAKE_CURRENT_LIST_DIR}/MetricDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/MetricRecord.cpp
        ${CMAKE_CURRENT_LIST_DIR}/MsgBridge.cpp
        ${CMAKE_CURRENT_LIST_DIR}/MsgProcessor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/PublisherDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/PublishMonitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/RecordDispatcher.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ReportDispatch.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TcpSocket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Uart.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UdpSocket.cpp
)

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/stream.h
        ${CMAKE_CURRENT_LIST_DIR}/AncillaryFields.h
        ${CMAKE_CURRENT_LIST_DIR}/CaptureDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/ClusterSocket.h
        ${CMAKE_CURRENT_LIST_DIR}/ContainerRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/CsvDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/DeviceIO.h
        ${CMAKE_CURRENT_LIST_DIR}/DeviceObject.h
        ${CMAKE_CURRENT_LIST_DIR}/DeviceReader.h
        ${CMAKE_CURRENT_LIST_DIR}/DeviceWriter.h
        ${CMAKE_CURRENT_LIST_DIR}/DispatchObject.h
        ${CMAKE_CURRENT_LIST_DIR}/File.h
        ${CMAKE_CURRENT_LIST_DIR}/HttpClient.h
        ${CMAKE_CURRENT_LIST_DIR}/LimitDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/LimitRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/MetricDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/MetricRecord.h
        ${CMAKE_CURRENT_LIST_DIR}/MsgBridge.h
        ${CMAKE_CURRENT_LIST_DIR}/MsgProcessor.h
        ${CMAKE_CURRENT_LIST_DIR}/PublisherDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/PublishMonitor.h
        ${CMAKE_CURRENT_LIST_DIR}/RecordDispatcher.h
        ${CMAKE_CURRENT_LIST_DIR}/ReportDispatch.h
        ${CMAKE_CURRENT_LIST_DIR}/TcpSocket.h
        ${CMAKE_CURRENT_LIST_DIR}/Uart.h
        ${CMAKE_CURRENT_LIST_DIR}/UdpSocket.h
    DESTINATION
        ${INCDIR}
)
