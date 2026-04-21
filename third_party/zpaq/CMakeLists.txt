cmake_minimum_required(VERSION 3.20)

add_library(libzpaq STATIC
  libzpaq/libzpaq.cpp
)

target_include_directories(libzpaq
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libzpaq
)

target_compile_features(libzpaq PUBLIC cxx_std_17)

# Required for linking into shared library (libmosqueeze.so)
set_target_properties(libzpaq PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(WIN32)
  target_link_libraries(libzpaq PUBLIC advapi32)
else()
  target_compile_definitions(libzpaq PRIVATE unix=1)
endif()
