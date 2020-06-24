file (GLOB SCRIPTS util/*.lua endpoint/*.lua client/*.py)
install (FILES ${SCRIPTS} DESTINATION ${CONFDIR})
