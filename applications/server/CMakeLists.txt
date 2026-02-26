message (STATUS "Building server application")

#####################
# Target: sliderule #
#####################

execute_process (COMMAND git --git-dir ${PROJECT_SOURCE_DIR}/.git --work-tree ${PROJECT_SOURCE_DIR} describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE COMMITINFO)
string(REGEX REPLACE "\n$" "" COMMITINFO "${COMMITINFO}")

execute_process(COMMAND uname -r OUTPUT_VARIABLE OSINFO)
string(REGEX REPLACE "\n$" "" OSINFO "${OSINFO}")

execute_process(COMMAND date OUTPUT_VARIABLE DATEINFO)
string(REGEX REPLACE "\n$" "" DATEINFO "${DATEINFO}")

set(BUILDINFO "${COMMITINFO}, ${OSINFO}, ${DATEINFO}")
target_compile_definitions (slideruleLib PUBLIC BUILDINFO="${BUILDINFO}")

add_executable (sliderule ${CMAKE_CURRENT_LIST_DIR}/SlideRule.cpp)

set_target_properties (sliderule PROPERTIES VERSION ${TGTVER})
set_target_properties (sliderule PROPERTIES OUTPUT_NAME sliderule)
set_target_properties (sliderule PROPERTIES ENABLE_EXPORTS true)

target_link_libraries (sliderule PUBLIC "-Wl,--whole-archive" slideruleLib "-Wl,--no-whole-archive")
install (TARGETS sliderule DESTINATION ${INSTALLDIR}/bin)
