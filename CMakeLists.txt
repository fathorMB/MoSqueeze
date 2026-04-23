cmake_minimum_required(VERSION 3.20)

# Enable automatic DLL copying for Windows
set(VCPKG_APPLOCAL_DEPS ON CACHE BOOL "Copy vcpkg DLLs to output directory" FORCE)

include(cmake/vcpkg.cmake OPTIONAL)

project(MoSqueeze VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MOSQUEEZE_BUILD_TESTS "Build unit tests" ON)
option(MOSQUEEZE_BUILD_SERVER "Build gRPC server" OFF)

if(MOSQUEEZE_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(third_party/zpaq)
add_subdirectory(src/libmosqueeze)
add_subdirectory(src/mosqueeze-cli)
add_subdirectory(src/mosqueeze-bench)
add_subdirectory(src/mosqueeze-viz)

if(MOSQUEEZE_BUILD_SERVER)
  add_subdirectory(src/mosqueeze-server)
endif()
if(MOSQUEEZE_BUILD_TESTS)
  add_subdirectory(tests)
endif()
