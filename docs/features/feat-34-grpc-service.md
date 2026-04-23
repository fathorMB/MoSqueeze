# Worker Spec: MoSqueeze gRPC Service (#34)

## Issue
https://github.com/fathorMB/TheVault/issues/34

## Dependencies
- ✅ `libmosqueeze` — Core compression library (exists)
- ✅ MoSqueeze CLI — Reference implementation (merged #67)

## Overview
Create a C++ gRPC microservice that exposes MoSqueeze compression capabilities via gRPC on port 50051. This replaces the subprocess-based approach with a proper service architecture.

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                         TheVault (Go)                            │
│  ┌─────────────────┐         gRPC          ┌────────────────────┐ │
│  │ mosqueeze.      │ ◄───────────────────►│ mosqueeze-server   │ │
│  │ HTTPClient      │    localhost:50051    │ (C++ :50051)       │ │
│  │ (future)        │                       │                    │ │
│  └─────────────────┘                       │  ┌──────────────┐  │ │
│                                            │  │ Compression   │  │ │
│  ┌─────────────────┐                       │  │ Pipeline      │  │ │
│  │ mosqueeze.      │   Current:            │  │ (libmosqueeze)│  │ │
│  │ CLIClient       │   os/exec → CLI       │  └──────────────┘  │ │
│  └─────────────────┘                       └────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

## File Structure

### New Directory: `src/mosqueeze-server/`

```
src/mosqueeze-server/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                 # Entry point + gRPC server setup
│   ├── service.cpp              # CompressionServiceImpl
│   ├── service.hpp
│   ├── compressor.cpp           # Wraps libmosqueeze
│   └── compressor.hpp
├── proto/
│   └── compression.proto        # gRPC service definition
└── tests/
    └── service_test.cpp         # Integration tests
```

### Root CMakeLists.txt Update

```cmake
option(MOSQUEEZE_BUILD_SERVER "Build gRPC server" OFF)

if(MOSQUEEZE_BUILD_SERVER)
  add_subdirectory(src/mosqueeze-server)
endif()
```

## Protocol Buffer Definition

### `src/mosqueeze-server/proto/compression.proto`

```protobuf
syntax = "proto3";

package mosqueeze;

// Compression service for cold storage
service Compression {
    // Synchronous compression (for small files < 10MB)
    rpc Compress(CompressRequest) returns (CompressResponse);
    
    // Synchronous decompression
    rpc Decompress(DecompressRequest) returns (DecompressResponse);
    
    // Streaming compression for large files
    rpc StreamCompress(stream StreamCompressRequest) returns (stream StreamCompressResponse);
    
    // Streaming decompression for large files
    rpc StreamDecompress(stream StreamDecompressRequest) returns (stream StreamDecompressResponse);
    
    // Get supported algorithms and their info
    rpc GetAlgorithms(GetAlgorithmsRequest) returns (GetAlgorithmsResponse);
    
    // Health check for load balancers
    rpc HealthCheck(HealthCheckRequest) returns (HealthCheckResponse);
}

// --- Messages ---

message CompressRequest {
    bytes payload = 1;              // Raw file content
    string algorithm = 2;           // "zstd", "brotli", "lzma", "zpaq"
    int32 level = 3;                 // Compression level (algorithm-specific)
    string preprocessor = 4;         // "none", "json-canonical", etc.
    map<string, string> options = 5; // Future extensions
}

message CompressResponse {
    bool success = 1;
    bytes payload = 2;              // Compressed content (.msz format)
    int64 input_size = 3;
    int64 output_size = 4;
    double ratio = 5;                // output_size / input_size
    string algorithm = 6;
    int32 level = 7;
    string preprocessor_used = 8;
    string error_message = 9;       // Only if success=false
}

message DecompressRequest {
    bytes payload = 1;              // Compressed content (.msz format)
    map<string, string> options = 2; // Future extensions
}

message DecompressResponse {
    bool success = 1;
    bytes payload = 2;              // Decompressed content
    int64 input_size = 3;
    int64 output_size = 4;
    string preprocessor_used = 5;    // Preprocessor that was applied
    string error_message = 6;
}

// Streaming messages for large files
message StreamCompressRequest {
    oneof data {
        FileMetadata metadata = 1;
        bytes chunk = 2;
    }
}

message FileMetadata {
    string filename = 1;
    string algorithm = 2;
    int32 level = 3;
    string preprocessor = 4;
    int64 expected_size = 5;        // For progress reporting
}

message StreamCompressResponse {
    oneof data {
        Progress progress = 1;
        CompressResponse result = 2;
    }
}

message Progress {
    int64 bytes_processed = 1;
    int64 total_bytes = 2;
    double percentage = 3;
}

message StreamDecompressRequest {
    oneof data {
        string filename = 1;        // Just filename for metadata
        bytes chunk = 2;
    }
}

message StreamDecompressResponse {
    oneof data {
        Progress progress = 1;
        DecompressResponse result = 2;
    }
}

// Algorithm discovery
message GetAlgorithmsRequest {}

message GetAlgorithmsResponse {
    repeated AlgorithmInfo algorithms = 1;
}

message AlgorithmInfo {
    string name = 1;                // "zstd", "brotli", "lzma", "zpaq"
    string display_name = 2;        // "Zstandard"
    int32 min_level = 3;
    int32 max_level = 4;
    int32 default_level = 5;
    string description = 6;
    repeated string supported_preprocessors = 7;
}

// Health check
message HealthCheckRequest {
    string service = 1;             // Optional: specific service name
}

message HealthCheckResponse {
    enum ServingStatus {
        UNKNOWN = 0;
        SERVING = 1;
        NOT_SERVING = 2;
    }
    ServingStatus status = 1;
    string version = 2;             // MoSqueeze version
    map<string, string> details = 3; // Extended info
}
```

## Implementation

### `src/mosqueeze-server/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)

# Find gRPC and Protobuf
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

# Generate protobuf and gRPC sources
set(PROTO_PATH ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_SRC ${CMAKE_CURRENT_BINARY_DIR}/proto)

protobuf_generate_cpp(
    ${PROTO_SRC}/compression.pb.h
    ${PROTO_SRC}/compression.pb.cc
    ${PROTO_PATH}/compression.proto
)

protobuf_generate_cpp_grpc(
    ${PROTO_SRC}/compression.grpc.pb.h
    ${PROTO_SRC}/compression.grpc.pb.cc
    ${PROTO_PATH}/compression.proto
)

# mosqueeze-server executable
add_executable(mosqueeze-server
    src/main.cpp
    src/service.cpp
    src/compressor.cpp
    ${PROTO_SRC}/compression.pb.cc
    ${PROTO_SRC}/compression.grpc.pb.cc
)

target_include_directories(mosqueeze-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROTO_SRC}
    ${CMAKE_SOURCE_DIR}/src/libmosqueeze/include
)

target_link_libraries(mosqueeze-server PRIVATE
    libmosqueeze
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
)

# Install
install(TARGETS mosqueeze-server DESTINATION bin)

# Tests (optional)
if(MOSQUEEZE_BUILD_TESTS)
    add_executable(server_test tests/service_test.cpp)
    target_link_libraries(server_test PRIVATE
        mosqueeze-server
        GTest::gtest
        GTest::grpc_test
    )
endif()
```

### `src/mosqueeze-server/src/compressor.hpp`

```cpp
#pragma once

#include <mosqueeze/CompressionPipeline.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <mosqueeze/engines/BrotliEngine.hpp>
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <mosqueeze/engines/ZpaqEngine.hpp>
#include <string>
#include <memory>

namespace mosqueeze::server {

struct CompressResult {
    bool success;
    std::vector<uint8_t> output;
    int64_t inputSize;
    int64_t outputSize;
    std::string algorithm;
    int level;
    std::string preprocessorUsed;
    std::string errorMessage;
};

struct DecompressResult {
    bool success;
    std::vector<uint8_t> output;
    int64_t inputSize;
    int64_t outputSize;
    std::string preprocessorUsed;
    std::string errorMessage;
};

class Compressor {
public:
    Compressor();
    
    CompressResult compress(
        const std::vector<uint8_t>& input,
        const std::string& algorithm,
        int level,
        const std::string& preprocessor);
    
    DecompressResult decompress(const std::vector<uint8_t>& input);
    
    static std::vector<std::string> supportedAlgorithms();
    static std::pair<int, int> algorithmLevelRange(const std::string& algorithm);
    static int defaultLevel(const std::string& algorithm);

private:
    ICompressionEngine* getEngine(const std::string& algorithm);
    
    ZstdEngine zstd_;
    BrotliEngine brotli_;
    LzmaEngine lzma_;
    ZpaqEngine zpaq_;
};

} // namespace mosqueeze::server
```

### `src/mosqueeze-server/src/compressor.cpp`

```cpp
#include "compressor.hpp"
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>
#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>
#include <mosqueeze/preprocessors/PngOptimizer.hpp>
#include <mosqueeze/preprocessors/BayerPreprocessor.hpp>
#include <sstream>
#include <stdexcept>

namespace mosqueeze::server {

Compressor::Compressor() = default;

CompressResult Compressor::compress(
    const std::vector<uint8_t>& input,
    const std::string& algorithm,
    int level,
    const std::string& preprocessor) {
    
    CompressResult result;
    result.inputSize = static_cast<int64_t>(input.size());
    result.algorithm = algorithm;
    result.level = level > 0 ? level : defaultLevel(algorithm);
    
    try {
        auto* engine = getEngine(algorithm);
        if (!engine) {
            result.success = false;
            result.errorMessage = "Unknown algorithm: " + algorithm;
            return result;
        }
        
        CompressionPipeline pipeline(engine);
        
        // Set preprocessor if specified
        std::unique_ptr<IPreprocessor> prep;
        if (preprocessor != "none" && !preprocessor.empty()) {
            if (preprocessor == "json-canonical") {
                prep = std::make_unique<JsonCanonicalizer>();
            } else if (preprocessor == "xml-canonical") {
                prep = std::make_unique<XmlCanonicalizer>();
            } else if (preprocessor == "image-meta-strip") {
                prep = std::make_unique<ImageMetaStripper>();
            } else if (preprocessor == "png-optimizer") {
                prep = std::make_unique<PngOptimizer>();
            } else if (preprocessor == "bayer-raw") {
                prep = std::make_unique<BayerPreprocessor>();
            }
            if (prep) {
                pipeline.setPreprocessor(prep.get());
            }
        }
        
        // Compress
        std::istringstream input_stream(
            std::string(input.begin(), input.end()),
            std::ios::binary);
        std::ostringstream output_stream(std::ios::binary);
        
        CompressionOptions opts;
        opts.level = result.level;
        
        auto pipeline_result = pipeline.compress(input_stream, output_stream, opts);
        
        // Extract output
        std::string compressed = output_stream.str();
        result.output.assign(compressed.begin(), compressed.end());
        result.outputSize = static_cast<int64_t>(result.output.size());
        result.ratio = static_cast<double>(result.outputSize) /
                       static_cast<double>(result.inputSize > 0 ? result.inputSize : 1);
        result.preprocessorUsed = preprocessor.empty() ? "none" : preprocessor;
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    
    return result;
}

DecompressResult Compressor::decompress(const std::vector<uint8_t>& input) {
    DecompressResult result;
    result.inputSize = static_cast<int64_t>(input.size());
    
    try {
        // Decompression uses trailing header to detect algorithm/preprocessor
        // We need to use a generic engine for detection
        
        std::istringstream input_stream(
            std::string(input.begin(), input.end()),
            std::ios::binary);
        std::ostringstream output_stream(std::ios::binary);
        
        // Try each engine until one works (or use metadata)
        // For now, we'll use zstd as default and let the trailing header guide
        CompressionPipeline pipeline(&zstd_);
        
        pipeline.decompress(input_stream, output_stream);
        
        std::string decompressed = output_stream.str();
        result.output.assign(decompressed.begin(), decompressed.end());
        result.outputSize = static_cast<int64_t>(result.output.size());
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    
    return result;
}

ICompressionEngine* Compressor::getEngine(const std::string& algorithm) {
    if (algorithm == "zstd") return &zstd_;
    if (algorithm == "brotli") return &brotli_;
    if (algorithm == "lzma") return &lzma_;
    if (algorithm == "zpaq") return &zpaq_;
    return nullptr;
}

std::vector<std::string> Compressor::supportedAlgorithms() {
    return {"zstd", "brotli", "lzma", "zpaq"};
}

std::pair<int, int> Compressor::algorithmLevelRange(const std::string& algorithm) {
    if (algorithm == "zstd") return {1, 22};
    if (algorithm == "brotli") return {0, 11};
    if (algorithm == "lzma") return {0, 9};
    if (algorithm == "zpaq") return {1, 5};
    return {0, 0};
}

int Compressor::defaultLevel(const std::string& algorithm) {
    // Conservative defaults for cold storage
    if (algorithm == "zstd") return 3;
    if (algorithm == "brotli") return 6;
    if (algorithm == "lzma") return 6;
    if (algorithm == "zpaq") return 3;
    return 3;
}

} // namespace mosqueeze::server
```

### `src/mosqueeze-server/src/service.hpp`

```cpp
#pragma once

#include "compression.grpc.pb.h"
#include "compressor.hpp"
#include <grpcpp/grpcpp.h>
#include <memory>

namespace mosqueeze::server {

class CompressionServiceImpl final : public Compression::Service {
public:
    explicit CompressionServiceImpl(std::shared_ptr<Compressor> compressor);
    
    grpc::Status Compress(
        grpc::ServerContext* context,
        const CompressRequest* request,
        CompressResponse* response) override;
    
    grpc::Status Decompress(
        grpc::ServerContext* context,
        const DecompressRequest* request,
        DecompressResponse* response) override;
    
    grpc::Status StreamCompress(
        grpc::ServerContext* context,
        grpc::ServerReaderWriter<StreamCompressResponse, StreamCompressRequest>* stream) override;
    
    grpc::Status StreamDecompress(
        grpc::ServerContext* context,
        grpc::ServerReaderWriter<StreamDecompressResponse, StreamDecompressRequest>* stream) override;
    
    grpc::Status GetAlgorithms(
        grpc::ServerContext* context,
        const GetAlgorithmsRequest* request,
        GetAlgorithmsResponse* response) override;
    
    grpc::Status HealthCheck(
        grpc::ServerContext* context,
        const HealthCheckRequest* request,
        HealthCheckResponse* response) override;

private:
    std::shared_ptr<Compressor> compressor_;
};

} // namespace mosqueeze::server
```

### `src/mosqueeze-server/src/main.cpp`

```cpp
#include "service.hpp"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <memory>

using grpc::ServerBuilder;
using grpc::Server;
using mosqueeze::server::CompressionServiceImpl;
using mosqueeze::server::Compressor;

int main(int argc, char** argv) {
    // Configuration from environment
    std::string host = std::getenv("MOSQUEEZE_HOST") ? std::getenv("MOSQUEEZE_HOST") : "0.0.0.0";
    int port = std::getenv("MOSQUEEZE_PORT") ? std::stoi(std::getenv("MOSQUEEZE_PORT")) : 50051;
    std::string log_level = std::getenv("MOSQUEEZE_LOG_LEVEL") ? std::getenv("MOSQUEEZE_LOG_LEVEL") : "info";
    
    // Set log level
    if (log_level == "debug") spdlog::set_level(spdlog::level::debug);
    else if (log_level == "trace") spdlog::set_level(spdlog::level::trace);
    else spdlog::set_level(spdlog::level::info);
    
    spdlog::info("MoSqueeze gRPC Server v{}", MOSQUEEZE_VERSION);
    spdlog::info("Listening on {}:{}", host, port);
    
    // Create compressor
    auto compressor = std::make_shared<Compressor>();
    
    // Create service
    CompressionServiceImpl service(compressor);
    
    // Build server
    std::string server_address = host + ":" + std::to_string(port);
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    // Start
    std::unique_ptr<Server> server(builder.BuildAndStart());
    if (!server) {
        spdlog::error("Failed to start server on {}", server_address);
        return 1;
    }
    
    spdlog::info("Server started successfully");
    server->Wait();
    
    return 0;
}
```

## vcpkg Dependencies

### `vcpkg.json` Update

```json
{
  "name": "mosqueeze",
  "version": "0.1.0",
  "dependencies": [
    "zstd",
    "lzma",
    "brotli",
    "cli11",
    "fmt",
    "spdlog",
    "nlohmann-json",
    "sqlite3",
    "pugixml",
    "zlib",
    "grpc",       // NEW
    "protobuf"    // NEW
  ]
}
```

## Dockerfile

### `src/mosqueeze-server/Dockerfile`

```dockerfile
# Build stage
FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build

# Install build tools
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    git \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# Copy source
COPY CMakeLists.txt .
COPY vcpkg.json .
COPY src/ src/
COPY third_party/ third_party/

# Build
RUN cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DMOSQUEEZE_BUILD_SERVER=ON \
    && cmake --build build --config Release -j$(nproc)

# Runtime stage
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /app/build/src/mosqueeze-server/mosqueeze-server /usr/local/bin/

EXPOSE 50051

HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
    CMD grpcurl -plaintext localhost:50051 grpc.health.v1.Health/Check || exit 1

ENTRYPOINT ["mosqueeze-server"]
```

## tests

### Key Test Cases

```cpp
// tests/service_test.cpp (integration test)

TEST(CompressionService, CompressDecompressRoundtrip) {
    // Start server on port 50052 (test port)
    auto compressor = std::make_shared<Compressor>();
    CompressionServiceImpl service(compressor);
    
    // Create test payload
    std::string test_data = "Hello, MoSqueeze! This is a test string for compression.";
    CompressRequest request;
    request.set_payload(test_data);
    request.set_algorithm("zstd");
    request.set_level(3);
    
    // Compress
    CompressResponse compress_response;
    auto status = service.Compress(nullptr, &request, &compress_response);
    
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(compress_response.success());
    EXPECT_LT(compress_response.output_size(), compress_response.input_size());
    
    // Decompress
    DecompressRequest decompress_request;
    decompress_request.set_payload(compress_response.payload());
    
    DecompressResponse decompress_response;
    status = service.Decompress(nullptr, &decompress_request, &decompress_response);
    
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(decompress_response.success());
    EXPECT_EQ(decompress_response.payload(), test_data);
}

TEST(CompressionService, AllAlgorithmsWork) {
    for (const auto& algo : {"zstd", "brotli", "lzma", "zpaq"}) {
        CompressRequest request;
        request.set_payload("Test data for compression");
        request.set_algorithm(algo);
        request.set_level(3);
        
        // ... test each algorithm
    }
}

TEST(CompressionService, HealthCheckReturnsServing) {
    HealthCheckRequest request;
    HealthCheckResponse response;
    
    auto status = service.HealthCheck(nullptr, &request, &response);
    
    ASSERT_TRUE(status.ok());
    EXPECT_EQ(response.status(), HealthCheckResponse::SERVING);
}
```

## Acceptance Criteria

- [ ] `src/mosqueeze-server/` directory structure created
- [ ] `proto/compression.proto` defines all messages and service
- [ ] CMakeLists.txt builds with gRPC/Protobuf dependencies
- [ ] `Compress` RPC works for all 4 algorithms
- [ ] `Decompress` RPC restores original content
- [ ] `GetAlgorithms` returns algorithm metadata
- [ ] `HealthCheck` returns SERVING status
- [ ] `StreamCompress` handles files > 10MB in chunks
- [ ] Dockerfile builds and runs on port 50051
- [ ] Integration test with Go client passes

## Build Commands

```bash
# Build with gRPC support
cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DMOSQUEEZE_BUILD_SERVER=ON

cmake --build build --config Release -j$(nproc)

# Run server
./build/src/mosqueeze-server/mosqueeze-server

# Test with grpcurl
grpcurl -plaintext localhost:50051 mosqueeze.Compression/HealthCheck
grpcurl -plaintext localhost:50051 mosqueeze.Compression/GetAlgorithms
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `MOSQUEEZE_HOST` | `0.0.0.0` | Server bind address |
| `MOSQUEEZE_PORT` | `50051` | Server port |
| `MOSQUEEZE_LOG_LEVEL` | `info` | Log level (trace, debug, info) |

## Future Enhancements (Out of Scope)

- TLS/SSL support
- Authentication (JWT, mTLS)
- Rate limiting
- Metrics endpoint (Prometheus)
- Compression job queue for async processing