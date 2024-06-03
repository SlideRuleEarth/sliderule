cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(ATL24_coastnet VERSION 1.0.0 LANGUAGES CXX)

find_package(OpenMP REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Torch REQUIRED)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

message(STATUS "Torch version ${Torch_VERSION}")

set(CMAKE_CXX_FLAGS "-Wall -Werror ${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

set(PI_NAME "index_ph")
set(X_NAME "x_atc")
set(Z_NAME "geoid_corr_h")

add_definitions(-DPI_NAME=\"${PI_NAME}\")
add_definitions(-DX_NAME=\"${X_NAME}\")
add_definitions(-DZ_NAME=\"${Z_NAME}\")

include_directories(${PROJECT_SOURCE_DIR})

add_executable(classify_coastnet ./apps/classify_coastnet.cpp)
target_link_libraries(classify_coastnet ${TORCH_LIBRARIES} gmp)
target_include_directories(classify_coastnet PUBLIC)
target_precompile_headers(classify_coastnet PUBLIC ATL24_coastnet/precompiled.h)

add_executable(blunder_detection ./apps/blunder_detection.cpp)
target_link_libraries(blunder_detection ${TORCH_LIBRARIES} gmp)
target_include_directories(blunder_detection PUBLIC)
target_precompile_headers(blunder_detection PUBLIC ATL24_coastnet/precompiled.h)

add_executable(combine_predictions ./apps/combine_predictions.cpp)
target_link_libraries(combine_predictions ${TORCH_LIBRARIES} gmp)
target_include_directories(combine_predictions PUBLIC)
target_precompile_headers(combine_predictions PUBLIC ATL24_coastnet/precompiled.h)
