message (STATUS "Including xdem package")

target_compile_definitions (slideruleLib PUBLIC __xdem__)

# Endpoints #
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/xdem.lua
    DESTINATION
        ${CONFDIR}/api
)
