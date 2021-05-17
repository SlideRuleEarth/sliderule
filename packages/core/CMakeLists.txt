find_package (Lua "5.3")
find_library (READLINE_LIB readline)

if (LUA_FOUND AND READLINE_LIB)

    message (STATUS "Including core package")

    target_compile_definitions (slideruleLib PUBLIC __core__)
    target_compile_definitions (slideruleLib PRIVATE LUA_USE_READLINE)

    target_link_libraries (slideruleLib PRIVATE ${LUA_LIBRARIES})
    target_include_directories (slideruleLib PUBLIC ${LUA_INCLUDE_DIR})

    target_link_libraries (slideruleLib PUBLIC readline)

    target_sources (slideruleLib
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/core.cpp
            ${CMAKE_CURRENT_LIST_DIR}/Asset.cpp
            ${CMAKE_CURRENT_LIST_DIR}/CaptureDispatch.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ClusterSocket.cpp
            ${CMAKE_CURRENT_LIST_DIR}/CsvDispatch.cpp
            ${CMAKE_CURRENT_LIST_DIR}/DeviceIO.cpp
            ${CMAKE_CURRENT_LIST_DIR}/DeviceObject.cpp
            ${CMAKE_CURRENT_LIST_DIR}/DeviceReader.cpp
            ${CMAKE_CURRENT_LIST_DIR}/DeviceWriter.cpp
            ${CMAKE_CURRENT_LIST_DIR}/DispatchObject.cpp
            ${CMAKE_CURRENT_LIST_DIR}/EndpointObject.cpp
            ${CMAKE_CURRENT_LIST_DIR}/EventLib.cpp
            ${CMAKE_CURRENT_LIST_DIR}/PointIndex.cpp
            ${CMAKE_CURRENT_LIST_DIR}/File.cpp
            ${CMAKE_CURRENT_LIST_DIR}/HttpClient.cpp
            ${CMAKE_CURRENT_LIST_DIR}/HttpServer.cpp
            ${CMAKE_CURRENT_LIST_DIR}/LimitDispatch.cpp
            ${CMAKE_CURRENT_LIST_DIR}/LimitRecord.cpp
            ${CMAKE_CURRENT_LIST_DIR}/LuaEndpoint.cpp
            ${CMAKE_CURRENT_LIST_DIR}/LuaEngine.cpp
            ${CMAKE_CURRENT_LIST_DIR}/LuaLibraryMsg.cpp
            ${CMAKE_CURRENT_LIST_DIR}/LuaLibrarySys.cpp
            ${CMAKE_CURRENT_LIST_DIR}/LuaLibraryTime.cpp
            ${CMAKE_CURRENT_LIST_DIR}/LuaObject.cpp
            ${CMAKE_CURRENT_LIST_DIR}/MathLib.cpp
            ${CMAKE_CURRENT_LIST_DIR}/MetricDispatch.cpp
            ${CMAKE_CURRENT_LIST_DIR}/MetricRecord.cpp
            ${CMAKE_CURRENT_LIST_DIR}/Monitor.cpp
            ${CMAKE_CURRENT_LIST_DIR}/MsgBridge.cpp
            ${CMAKE_CURRENT_LIST_DIR}/MsgProcessor.cpp
            ${CMAKE_CURRENT_LIST_DIR}/MsgQ.cpp
            ${CMAKE_CURRENT_LIST_DIR}/PublisherDispatch.cpp
            ${CMAKE_CURRENT_LIST_DIR}/RecordObject.cpp
            ${CMAKE_CURRENT_LIST_DIR}/RecordDispatcher.cpp
            ${CMAKE_CURRENT_LIST_DIR}/ReportDispatch.cpp
            ${CMAKE_CURRENT_LIST_DIR}/SpatialIndex.cpp
            ${CMAKE_CURRENT_LIST_DIR}/StringLib.cpp
            ${CMAKE_CURRENT_LIST_DIR}/TcpSocket.cpp
            ${CMAKE_CURRENT_LIST_DIR}/IntervalIndex.cpp
            ${CMAKE_CURRENT_LIST_DIR}/TimeLib.cpp
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
            ${CMAKE_CURRENT_LIST_DIR}/core.h
            ${CMAKE_CURRENT_LIST_DIR}/Asset.h
            ${CMAKE_CURRENT_LIST_DIR}/AssetIndex.h
            ${CMAKE_CURRENT_LIST_DIR}/CaptureDispatch.h
            ${CMAKE_CURRENT_LIST_DIR}/ClusterSocket.h
            ${CMAKE_CURRENT_LIST_DIR}/CsvDispatch.h
            ${CMAKE_CURRENT_LIST_DIR}/DeviceIO.h
            ${CMAKE_CURRENT_LIST_DIR}/DeviceObject.h
            ${CMAKE_CURRENT_LIST_DIR}/DeviceReader.h
            ${CMAKE_CURRENT_LIST_DIR}/DeviceWriter.h
            ${CMAKE_CURRENT_LIST_DIR}/Dictionary.h
            ${CMAKE_CURRENT_LIST_DIR}/DispatchObject.h
            ${CMAKE_CURRENT_LIST_DIR}/EndpointObject.h
            ${CMAKE_CURRENT_LIST_DIR}/PointIndex.h
            ${CMAKE_CURRENT_LIST_DIR}/File.h
            ${CMAKE_CURRENT_LIST_DIR}/HttpClient.h
            ${CMAKE_CURRENT_LIST_DIR}/HttpServer.h
            ${CMAKE_CURRENT_LIST_DIR}/LimitDispatch.h
            ${CMAKE_CURRENT_LIST_DIR}/LimitRecord.h
            ${CMAKE_CURRENT_LIST_DIR}/List.h
            ${CMAKE_CURRENT_LIST_DIR}/EventLib.h
            ${CMAKE_CURRENT_LIST_DIR}/LuaEndpoint.h
            ${CMAKE_CURRENT_LIST_DIR}/LuaEngine.h
            ${CMAKE_CURRENT_LIST_DIR}/LuaLibraryMsg.h
            ${CMAKE_CURRENT_LIST_DIR}/LuaLibrarySys.h
            ${CMAKE_CURRENT_LIST_DIR}/LuaLibraryTime.h
            ${CMAKE_CURRENT_LIST_DIR}/LuaObject.h
            ${CMAKE_CURRENT_LIST_DIR}/MathLib.h
            ${CMAKE_CURRENT_LIST_DIR}/MetricDispatch.h
            ${CMAKE_CURRENT_LIST_DIR}/MetricRecord.h
            ${CMAKE_CURRENT_LIST_DIR}/Monitor.h
            ${CMAKE_CURRENT_LIST_DIR}/MsgBridge.h
            ${CMAKE_CURRENT_LIST_DIR}/MsgProcessor.h
            ${CMAKE_CURRENT_LIST_DIR}/MsgQ.h
            ${CMAKE_CURRENT_LIST_DIR}/Ordering.h
            ${CMAKE_CURRENT_LIST_DIR}/PublisherDispatch.h
            ${CMAKE_CURRENT_LIST_DIR}/RecordObject.h
            ${CMAKE_CURRENT_LIST_DIR}/RecordDispatcher.h
            ${CMAKE_CURRENT_LIST_DIR}/ReportDispatch.h
            ${CMAKE_CURRENT_LIST_DIR}/SpatialIndex.h
            ${CMAKE_CURRENT_LIST_DIR}/StringLib.h
            ${CMAKE_CURRENT_LIST_DIR}/Table.h
            ${CMAKE_CURRENT_LIST_DIR}/TcpSocket.h
            ${CMAKE_CURRENT_LIST_DIR}/IntervalIndex.h
            ${CMAKE_CURRENT_LIST_DIR}/TimeLib.h
            ${CMAKE_CURRENT_LIST_DIR}/Uart.h
            ${CMAKE_CURRENT_LIST_DIR}/UdpSocket.h
        DESTINATION
            ${INCDIR}
    )

    install (
        FILES
            ${CMAKE_CURRENT_LIST_DIR}/leap-seconds.list
        DESTINATION
            ${CONFDIR}
    )

else ()

    message (STATUS "Unable to compile core package... Lua library not found")

endif ()
