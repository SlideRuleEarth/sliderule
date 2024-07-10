cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(ATL24_qtrees VERSION 1.0.0 LANGUAGES CXX)

find_package(OpenMP REQUIRED)
find_package(xgboost REQUIRED)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(CMAKE_CXX_FLAGS "-Wall -Werror ${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

set(PI_NAME "index_ph")
set(X_NAME "x_atc")
set(Z_NAME "ortho_h")

add_definitions(-DPI_NAME=\"${PI_NAME}\")
add_definitions(-DX_NAME=\"${X_NAME}\")
add_definitions(-DZ_NAME=\"${Z_NAME}\")

include_directories(${PROJECT_SOURCE_DIR})

############################################################
# Applications
############################################################
add_executable(classify ./apps/classify.cpp)
target_link_libraries(classify xgboost::xgboost)
target_precompile_headers(classify PUBLIC apps/precompiled.h)
