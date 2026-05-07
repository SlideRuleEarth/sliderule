install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/server.lua
        ${CMAKE_CURRENT_LIST_DIR}/job_runner.lua
        ${CMAKE_CURRENT_LIST_DIR}/test_runner.lua
        ${CMAKE_CURRENT_LIST_DIR}/openapi.lua
    DESTINATION
        ${CONFDIR}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/index.yml
    DESTINATION
        ${CONFDIR}/openapi
)