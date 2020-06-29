file (GLOB SCRIPTS 
    endpoints/*.lua
    extensions/*.lua
)

install (FILES ${SCRIPTS} DESTINATION ${CONFDIR})
