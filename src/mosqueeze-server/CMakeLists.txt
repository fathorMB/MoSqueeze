cmake_minimum_required(VERSION 3.20)

find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/compression.proto)
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

file(MAKE_DIRECTORY ${GEN_DIR})

if(TARGET gRPC::grpc_cpp_plugin)
  set(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
else()
  find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
endif()

set(PROTO_SRCS ${GEN_DIR}/compression.pb.cc ${GEN_DIR}/compression.grpc.pb.cc)
set(PROTO_HDRS ${GEN_DIR}/compression.pb.h ${GEN_DIR}/compression.grpc.pb.h)

add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
  COMMAND protobuf::protoc
  ARGS --proto_path=${PROTO_DIR}
       --cpp_out=${GEN_DIR}
       --grpc_out=${GEN_DIR}
       --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
       ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  COMMENT "Generating gRPC/protobuf sources for compression.proto"
  VERBATIM
)

add_library(mosqueeze-server-lib STATIC
  src/compressor.cpp
  src/service.cpp
  ${PROTO_SRCS}
)

target_include_directories(mosqueeze-server-lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GEN_DIR}
)

target_link_libraries(mosqueeze-server-lib
  PUBLIC
    libmosqueeze
    gRPC::grpc++
    protobuf::libprotobuf
)

add_executable(mosqueeze-server
  src/main.cpp
)

target_link_libraries(mosqueeze-server
  PRIVATE
    mosqueeze-server-lib
    spdlog::spdlog
)

if(WIN32)
  add_custom_command(TARGET mosqueeze-server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:libmosqueeze>"
            "$<TARGET_FILE_DIR:mosqueeze-server>"
  )
endif()

install(TARGETS mosqueeze-server DESTINATION bin)

if(MOSQUEEZE_BUILD_TESTS)
  add_executable(mosqueeze-server-test
    tests/service_test.cpp
  )
  target_link_libraries(mosqueeze-server-test PRIVATE mosqueeze-server-lib)
  add_test(NAME mosqueeze-server-test COMMAND mosqueeze-server-test)
endif()
