file (GLOB SCRIPTS 
    endpoints/*
    extensions/*
)

install (FILES ${SCRIPTS} DESTINATION ${CONFDIR})
