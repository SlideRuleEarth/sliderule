message (STATUS "Using linux platform")

########################
# Target: slideruleLib #
########################

# Additional Build Information #

execute_process (COMMAND git --git-dir ${PROJECT_SOURCE_DIR}/.git --work-tree ${PROJECT_SOURCE_DIR} describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE COMMITINFO)
string(REGEX REPLACE "\n$" "" COMMITINFO "${COMMITINFO}")

execute_process(COMMAND uname -r OUTPUT_VARIABLE OSINFO)
string(REGEX REPLACE "\n$" "" OSINFO "${OSINFO}")

execute_process(COMMAND date OUTPUT_VARIABLE DATEINFO)
string(REGEX REPLACE "\n$" "" DATEINFO "${DATEINFO}")

set(BUILDINFO "${COMMITINFO}, ${OSINFO}, ${DATEINFO}")
target_compile_definitions (slideruleLib PUBLIC BUILDINFO="${BUILDINFO}")

# Compile Options #

target_compile_options (slideruleLib PUBLIC -pthread) # posix threads used in linux build
target_compile_options (slideruleLib PUBLIC -Wall) # turn on "all" warnings
target_compile_options (slideruleLib PUBLIC -Wextra) # turn on "extra" warnings
target_compile_options (slideruleLib PUBLIC -Wreorder) # turn on warning for object initializer list order enforcement
target_compile_options (slideruleLib PUBLIC -Wshadow) # turn on warning for inner scope var with same name as outer scope var
target_compile_options (slideruleLib PUBLIC "$<$<CONFIG:Debug>:-fstack-protector-all>")
target_compile_definitions (slideruleLib PUBLIC "$<$<CONFIG:Debug>:__no_unload__>") # don't unload dynamic modules when in debug so symbols available to address sanitizer postmortem

if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (slideruleLib PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

if(ENABLE_CODE_COVERAGE)
    target_compile_options (slideruleLib PUBLIC "$<$<CONFIG:Debug>:-fprofile-arcs>")
    target_compile_options (slideruleLib PUBLIC "$<$<CONFIG:Debug>:-ftest-coverage>")
endif()

# Link Options #

target_link_libraries (slideruleLib PUBLIC dl)
target_link_libraries (slideruleLib PUBLIC pthread)
target_link_libraries (slideruleLib PUBLIC m)
target_link_libraries (slideruleLib PUBLIC rt)

if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (slideruleLib PUBLIC -fsanitize=address)
endif()

if(ENABLE_CODE_COVERAGE)
    target_link_libraries (slideruleLib PUBLIC "$<$<CONFIG:Debug>:gcov>")
    target_link_options (slideruleLib PUBLIC "$<$<CONFIG:Debug>:--coverage>")
endif()

# Sources #

target_sources (slideruleLib
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/OsApi.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Cond.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Mutex.cpp
        ${CMAKE_CURRENT_LIST_DIR}/RunTimeException.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Sem.cpp
        ${CMAKE_CURRENT_LIST_DIR}/SockLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Thread.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Timer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TTYLib.cpp
)

# Includes #

target_include_directories (slideruleLib
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

# Installation #

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/OsApi.h
        ${CMAKE_CURRENT_LIST_DIR}/Cond.h
        ${CMAKE_CURRENT_LIST_DIR}/Mutex.h
        ${CMAKE_CURRENT_LIST_DIR}/RunTimeException.h
        ${CMAKE_CURRENT_LIST_DIR}/Sem.h
        ${CMAKE_CURRENT_LIST_DIR}/SockLib.h
        ${CMAKE_CURRENT_LIST_DIR}/Thread.h
        ${CMAKE_CURRENT_LIST_DIR}/Timer.h
        ${CMAKE_CURRENT_LIST_DIR}/TTYLib.h
    DESTINATION
        ${INCDIR}
)

#####################
# Target: sliderule #
#####################

add_executable (sliderule ${CMAKE_CURRENT_LIST_DIR}/SlideRule.cpp)

set_target_properties (sliderule PROPERTIES VERSION ${TGTVER})
set_target_properties (sliderule PROPERTIES OUTPUT_NAME sliderule)
set_target_properties (sliderule PROPERTIES ENABLE_EXPORTS true)

target_link_libraries (sliderule PUBLIC "-Wl,--whole-archive" slideruleLib "-Wl,--no-whole-archive")
install (TARGETS sliderule DESTINATION ${INSTALLDIR}/bin)
