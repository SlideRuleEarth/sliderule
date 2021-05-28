
# Endpoints #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/definition.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/example_engine_endpoint.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/example_source_endpoint.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/h5.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/h5p.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/index.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/geo.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/event.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/tail.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/metric.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/time.lua
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/version.lua
    DESTINATION
        ${CONFDIR}
)

# Extensions #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/extensions/asset.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/console.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/csv.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/global.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/json.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/packet.lua
        ${CMAKE_CURRENT_LIST_DIR}/extensions/test_executive.lua
    DESTINATION
        ${CONFDIR}
)
