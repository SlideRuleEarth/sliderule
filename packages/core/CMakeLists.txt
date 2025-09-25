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
target_compile_definitions (slideruleLib PUBLIC "$<$<CONFIG:Release>:LUA_USE_READLINE>")
target_link_libraries (slideruleLib PUBLIC ${READLINE_LIB})

# sources
target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/package/core.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/AlertMonitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/AmsLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Asset.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/AssetField.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/CurlLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/EndpointObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/EndpointProxy.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/EventLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/FileIODriver.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoDataFrame.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/HttpServer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/IntervalIndex.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LogMonitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaEndpoint.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaEngine.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaLibraryMsg.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaLibrarySys.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaLibraryTime.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaScript.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ManagerLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/MathLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/Monitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/MsgQ.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/OrchestratorLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/OutputFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/OutputLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/PointIndex.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/ProvisioningSystemLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/RecordObject.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/RegionMask.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/RequestFields.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/RequestMetrics.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/SpatialIndex.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/StringLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/SystemConfig.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/TelemetryMonitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/TimeLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/package/UnitTest.cpp
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/unittests/UT_Dictionary.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/unittests/UT_Field.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/unittests/UT_List.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/unittests/UT_MsgQ.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/unittests/UT_Ordering.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/unittests/UT_String.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/unittests/UT_Table.cpp>
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_LIST_DIR}/unittests/UT_TimeLib.cpp>
)

# includes
target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/package>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/unittests>
)

# header install
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/package/core.h
        ${CMAKE_CURRENT_LIST_DIR}/package/AlertMonitor.h
        ${CMAKE_CURRENT_LIST_DIR}/package/AmsLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Asset.h
        ${CMAKE_CURRENT_LIST_DIR}/package/AssetField.h
        ${CMAKE_CURRENT_LIST_DIR}/package/AssetIndex.h
        ${CMAKE_CURRENT_LIST_DIR}/package/CurlLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Dictionary.h
        ${CMAKE_CURRENT_LIST_DIR}/package/EndpointObject.h
        ${CMAKE_CURRENT_LIST_DIR}/package/EndpointProxy.h
        ${CMAKE_CURRENT_LIST_DIR}/package/EventLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Field.h
        ${CMAKE_CURRENT_LIST_DIR}/package/FieldArray.h
        ${CMAKE_CURRENT_LIST_DIR}/package/FieldColumn.h
        ${CMAKE_CURRENT_LIST_DIR}/package/FieldDictionary.h
        ${CMAKE_CURRENT_LIST_DIR}/package/FieldElement.h
        ${CMAKE_CURRENT_LIST_DIR}/package/FieldEnumeration.h
        ${CMAKE_CURRENT_LIST_DIR}/package/FieldList.h
        ${CMAKE_CURRENT_LIST_DIR}/package/FieldMap.h
        ${CMAKE_CURRENT_LIST_DIR}/package/FileIODriver.h
        ${CMAKE_CURRENT_LIST_DIR}/package/GeoDataFrame.h
        ${CMAKE_CURRENT_LIST_DIR}/package/HttpServer.h
        ${CMAKE_CURRENT_LIST_DIR}/package/IntervalIndex.h
        ${CMAKE_CURRENT_LIST_DIR}/package/List.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LogMonitor.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaEndpoint.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaEngine.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaLibraryMsg.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaLibrarySys.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaLibraryTime.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaObject.h
        ${CMAKE_CURRENT_LIST_DIR}/package/LuaScript.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ManagerLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/MathLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Monitor.h
        ${CMAKE_CURRENT_LIST_DIR}/package/MsgQ.h
        ${CMAKE_CURRENT_LIST_DIR}/package/OrchestratorLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Ordering.h
        ${CMAKE_CURRENT_LIST_DIR}/package/OutputFields.h
        ${CMAKE_CURRENT_LIST_DIR}/package/OutputLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/PointIndex.h
        ${CMAKE_CURRENT_LIST_DIR}/package/ProvisioningSystemLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/RecordObject.h
        ${CMAKE_CURRENT_LIST_DIR}/package/RegionMask.h
        ${CMAKE_CURRENT_LIST_DIR}/package/RequestFields.h
        ${CMAKE_CURRENT_LIST_DIR}/package/RequestMetrics.h
        ${CMAKE_CURRENT_LIST_DIR}/package/SpatialIndex.h
        ${CMAKE_CURRENT_LIST_DIR}/package/StringLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/SystemConfig.h
        ${CMAKE_CURRENT_LIST_DIR}/package/Table.h
        ${CMAKE_CURRENT_LIST_DIR}/package/TelemetryMonitor.h
        ${CMAKE_CURRENT_LIST_DIR}/package/TimeLib.h
        ${CMAKE_CURRENT_LIST_DIR}/package/UnitTest.h
    DESTINATION
        ${INCDIR}
)

# config install
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/data/leap-seconds.list
    DESTINATION
        ${CONFDIR}
)

# Extensions #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/extensions/asset.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/base64.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/console.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/csv.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/dataframe.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/global.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/iam_role_auth.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/earth_data_auth.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/earth_data_query.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/json.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/packet.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/prettyprint.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/service_registry.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/test_executive.lua
    DESTINATION
        ${CONFDIR}/ext
)

# Endpoints #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/ace.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/ams.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/assets.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/defaults.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/definition.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/earthdata.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/event.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/geo.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/health.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/index.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/prometheus.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/time.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/version.lua
    DESTINATION
        ${CONFDIR}/api
)
