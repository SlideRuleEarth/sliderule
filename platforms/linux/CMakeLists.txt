message (STATUS "Using linux platform")

###################
# Target: sliderule #
###################

target_compile_definitions (sliderule PUBLIC _LINUX_)

# Additional Build Information #

execute_process (COMMAND git describe --abbrev --dirty --always --tags --long OUTPUT_VARIABLE BUILDINFO)
string(REGEX REPLACE "\n$" "" BUILDINFO "${BUILDINFO}")
target_compile_definitions (sliderule PUBLIC BUILDINFO="${BUILDINFO}")

# Compile Options #

target_compile_options (sliderule PUBLIC -pthread) # posix threads used in linux build
target_compile_options (sliderule PUBLIC -Wall) # turn on "all" warnings
target_compile_options (sliderule PUBLIC -Wextra) # turn on "extra" warnings
target_compile_options (sliderule PUBLIC -Wreorder) # turn on warning for object initializer list order enforcement
target_compile_options (sliderule PUBLIC -Wshadow) # turn on warning for inner scope var with same name as outer scope var
target_compile_options (sliderule PUBLIC "$<$<CONFIG:Debug>:-fprofile-arcs>")
target_compile_options (sliderule PUBLIC "$<$<CONFIG:Debug>:-ftest-coverage>")
target_compile_options (sliderule PUBLIC "$<$<CONFIG:Debug>:-fstack-protector-all>")

if(ENABLE_ADDRESS_SANITIZER)
	target_compile_options (sliderule PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

# Link Options #

target_link_libraries (sliderule PUBLIC dl)
target_link_libraries (sliderule PUBLIC pthread)
target_link_libraries (sliderule PUBLIC m)
target_link_libraries (sliderule PUBLIC rt)
target_link_libraries (sliderule PUBLIC "$<$<CONFIG:Debug>:gcov>")

target_link_options (sliderule PUBLIC "$<$<CONFIG:Debug>:--coverage>")

if(ENABLE_ADDRESS_SANITIZER)
	target_link_options (sliderule PUBLIC -fsanitize=address)
endif()

# Sources #

target_sources (sliderule
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/Cond.cpp
        ${CMAKE_CURRENT_LIST_DIR}/LocalLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Mutex.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Sem.cpp
        ${CMAKE_CURRENT_LIST_DIR}/SockLib.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Thread.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Timer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/TTYLib.cpp
)

# Includes #

target_include_directories (sliderule
    PUBLIC
        $<INSTALL_INTERFACE:${INCDIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

# Installation #

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/OsApi.h
        ${CMAKE_CURRENT_LIST_DIR}/Cond.h
        ${CMAKE_CURRENT_LIST_DIR}/LocalLib.h
        ${CMAKE_CURRENT_LIST_DIR}/Mutex.h
        ${CMAKE_CURRENT_LIST_DIR}/Sem.h
        ${CMAKE_CURRENT_LIST_DIR}/SockLib.h
        ${CMAKE_CURRENT_LIST_DIR}/Thread.h
        ${CMAKE_CURRENT_LIST_DIR}/Timer.h
        ${CMAKE_CURRENT_LIST_DIR}/TTYLib.h
    DESTINATION
        ${INCDIR}
)
