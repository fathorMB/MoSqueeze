cmake_minimum_required(VERSION 3.20)

include(cmake/vcpkg.cmake OPTIONAL)

project(MoSqueeze VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MOSQUEEZE_BUILD_TESTS "Build unit tests" ON)

add_subdirectory(src/libmosqueeze)
add_subdirectory(src/mosqueeze-cli)
add_subdirectory(src/mosqueeze-bench)

if(MOSQUEEZE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
