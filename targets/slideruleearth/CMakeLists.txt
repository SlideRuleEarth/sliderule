install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/server.lua
        ${CMAKE_CURRENT_LIST_DIR}/job_runner.lua
        ${CMAKE_CURRENT_LIST_DIR}/test_runner.lua
    DESTINATION
        ${CONFDIR}
)