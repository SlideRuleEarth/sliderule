# dependencies
find_package (Lua "5.3" REQUIRED)
find_package (CURL REQUIRED)
find_package (RapidJSON REQUIRED)
find_library (READLINE_LIB readline REQUIRED)

# core
message (STATUS "Including core package")
target_compile_definitions (slideruleLib PUBLIC __core__)

# lua
target_link_libraries (slideruleLib PUBLIC ${LUA_LIBRARIES})
target_include_directories (slideruleLib PUBLIC ${LUA_INCLUDE_DIR})

# curl
target_link_libraries (slideruleLib PUBLIC ${CURL_LIBRARIES})
target_include_directories (slideruleLib PUBLIC ${CURL_INCLUDE_DIR})

# rapidjson
target_include_directories (slideruleLib PUBLIC ${RapidJSON_INCLUDE_DIR})

# readline
target_compile_definitions (slideruleLib PUBLIC LUA_USE_READLINE)
target_link_libraries (slideruleLib PUBLIC ${READLINE_LIB})

# sources
target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/core.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Asset.cpp
        ${CMAKE_CURRENT_LIST_DIR}/CurlLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/EndpointObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/EndpointProxy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/EventLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/FieldDictionary.cpp
        ${CMAKE_CURRENT_LIST_DIR}/FileIODriver.cpp
        ${CMAKE_CURRENT_LIST_DIR}/GeoDataFrame.cpp
        ${CMAKE_CURRENT_LIST_DIR}/HttpServer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LuaEndpoint.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LuaEngine.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LuaLibraryMsg.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LuaLibrarySys.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LuaLibraryTime.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LuaObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LuaScript.cpp
        ${CMAKE_CURRENT_LIST_DIR}/MathLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/MetricMonitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Monitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/MsgQ.cpp
        ${CMAKE_CURRENT_LIST_DIR}/OrchestratorLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/PointIndex.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ProvisioningSystemLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/PublishMonitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/RecordObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/RegionMask.cpp
        ${CMAKE_CURRENT_LIST_DIR}/RequestFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/RequestMetrics.cpp
        ${CMAKE_CURRENT_LIST_DIR}/RequestParms.cpp
        ${CMAKE_CURRENT_LIST_DIR}/SpatialIndex.cpp
        ${CMAKE_CURRENT_LIST_DIR}/StringLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/IntervalIndex.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TimeLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UnitTest.cpp
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_Dictionary.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_Field.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_List.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_MsgQ.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_Ordering.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_String.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_Table.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_TimeLib.cpp>
)

# includes
target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

# header install
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/core.h
        ${CMAKE_CURRENT_LIST_DIR}/Asset.h
        ${CMAKE_CURRENT_LIST_DIR}/AssetIndex.h
        ${CMAKE_CURRENT_LIST_DIR}/CurlLib.h
        ${CMAKE_CURRENT_LIST_DIR}/Dictionary.h
        ${CMAKE_CURRENT_LIST_DIR}/EndpointObject.h
        ${CMAKE_CURRENT_LIST_DIR}/EndpointProxy.h
        ${CMAKE_CURRENT_LIST_DIR}/EventLib.h
        ${CMAKE_CURRENT_LIST_DIR}/Field.h
        ${CMAKE_CURRENT_LIST_DIR}/FieldArray.h
        ${CMAKE_CURRENT_LIST_DIR}/FieldColumn.h
        ${CMAKE_CURRENT_LIST_DIR}/FieldDictionary.h
        ${CMAKE_CURRENT_LIST_DIR}/FieldElement.h
        ${CMAKE_CURRENT_LIST_DIR}/FieldEnumeration.h
        ${CMAKE_CURRENT_LIST_DIR}/FieldList.h
        ${CMAKE_CURRENT_LIST_DIR}/FileIODriver.h
        ${CMAKE_CURRENT_LIST_DIR}/GeoDataFrame.h
        ${CMAKE_CURRENT_LIST_DIR}/HttpServer.h
        ${CMAKE_CURRENT_LIST_DIR}/List.h
        ${CMAKE_CURRENT_LIST_DIR}/LuaEndpoint.h
        ${CMAKE_CURRENT_LIST_DIR}/LuaEngine.h
        ${CMAKE_CURRENT_LIST_DIR}/LuaLibraryMsg.h
        ${CMAKE_CURRENT_LIST_DIR}/LuaLibrarySys.h
        ${CMAKE_CURRENT_LIST_DIR}/LuaLibraryTime.h
        ${CMAKE_CURRENT_LIST_DIR}/LuaObject.h
        ${CMAKE_CURRENT_LIST_DIR}/LuaScript.h
        ${CMAKE_CURRENT_LIST_DIR}/MathLib.h
        ${CMAKE_CURRENT_LIST_DIR}/MetricMonitor.h
        ${CMAKE_CURRENT_LIST_DIR}/Monitor.h
        ${CMAKE_CURRENT_LIST_DIR}/MsgQ.h
        ${CMAKE_CURRENT_LIST_DIR}/OrchestratorLib.h
        ${CMAKE_CURRENT_LIST_DIR}/Ordering.h
        ${CMAKE_CURRENT_LIST_DIR}/PointIndex.h
        ${CMAKE_CURRENT_LIST_DIR}/ProvisioningSystemLib.h
        ${CMAKE_CURRENT_LIST_DIR}/PublishMonitor.h
        ${CMAKE_CURRENT_LIST_DIR}/RecordObject.h
        ${CMAKE_CURRENT_LIST_DIR}/RegionMask.h
        ${CMAKE_CURRENT_LIST_DIR}/RequestFields.h
        ${CMAKE_CURRENT_LIST_DIR}/RequestMetrics.h
        ${CMAKE_CURRENT_LIST_DIR}/RequestParms.h
        ${CMAKE_CURRENT_LIST_DIR}/SpatialIndex.h
        ${CMAKE_CURRENT_LIST_DIR}/StringLib.h
        ${CMAKE_CURRENT_LIST_DIR}/Table.h
        ${CMAKE_CURRENT_LIST_DIR}/IntervalIndex.h
        ${CMAKE_CURRENT_LIST_DIR}/TimeLib.h
        ${CMAKE_CURRENT_LIST_DIR}/UnitTest.h
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_Dictionary.h>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_Field.h>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_List.h>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_MsgQ.h>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_Ordering.h>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_String.h>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_Table.h>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/UT_TimeLib.h>
    DESTINATION
        ${INCDIR}
)

# config install
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/leap-seconds.list
    DESTINATION
        ${CONFDIR}
)
