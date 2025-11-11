cmake_minimum_required(VERSION 3.16)
project(column_sketches LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Enable higher warnings
if (MSVC)
add_compile_options(/W4)
else()
add_compile_options(-Wall -Wextra -Wpedantic)
endif()


# Library is header-only for now
add_library(csketch INTERFACE)
target_include_directories(csketch INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)


# Small smoke test app
add_executable(smoke apps/smoke.cpp)
target_link_libraries(smoke PRIVATE csketch)

# build_sketch app
add_executable(build_sketch apps/build_sketch.cpp)
target_link_libraries(build_sketch PRIVATE csketch)

# Release defaults
if (NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()