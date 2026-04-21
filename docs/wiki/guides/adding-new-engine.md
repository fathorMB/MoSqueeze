# Adding a New Compression Engine

**Summary**: Step-by-step guide per aggiungere un nuovo algoritmo di compressione.

**Last updated**: 2026-04-22

---

## Overview

MoSqueeze supporta aggiungere nuovi engine implementando l'interfaccia `ICompressionEngine`.

### Example: Adding zpaq engine

---

## Step 1: Create Header

`src/libmosqueeze/include/mosqueeze/engines/ZpaqEngine.hpp`:

```cpp
#pragma once

#include <mosqueeze/ICompressionEngine.hpp>
#include <vector>

namespace mosqueeze {

class ZpaqEngine : public ICompressionEngine {
public:
    // Metadata
    std::string name() const override { return "zpaq"; }
    std::string fileExtension() const override { return ".zpaq"; }
    std::vector<int> supportedLevels() const override;
    int defaultLevel() const override { return 5; }
    int maxLevel() const override { return 5; }

    // Operations
    CompressionResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {}) override;

    void decompress(
        std::istream& input,
        std::ostream& output) override;
};

} // namespace mosqueeze
```

---

## Step 2: Create Implementation

`src/libmosqueeze/src/engines/ZpaqEngine.cpp`:

```cpp
#include <mosqueeze/engines/ZpaqEngine.hpp>

#include "platform/Platform.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>

// Include zpaq headers
#include <libzpaq.h>

namespace mosqueeze {
namespace {

constexpr size_t BUFFER_SIZE = 64 * 1024;

void ensureStreamGood(const std::ios& stream, const char* context) {
    if (!stream.good() && !stream.eof()) {
        throw std::runtime_error(std::string("I/O error during ") + context);
    }
}

} // namespace

std::vector<int> ZpaqEngine::supportedLevels() const {
    // Zpaq has levels 1-5
    return {1, 2, 3, 4, 5};
}

CompressionResult ZpaqEngine::compress(
    std::istream& input,
    std::ostream& output,
    const CompressionOptions& opts) {
    
    auto startedAt = std::chrono::steady_clock::now();
    
    int level = opts.level > 0 ? std::clamp(opts.level, 1, 5) : defaultLevel();
    
    CompressionResult result{};
    
    // Initialize zpaq compressor
    // ... zpaq-specific streaming API ...
    
    std::array<char, BUFFER_SIZE> buffer{};
    
    while (true) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto readCount = static_cast<size_t>(input.gcount());
        result.originalBytes += readCount;
        
        // Process chunk with zpaq
        // ...
        
        if (input.eof()) break;
        ensureStreamGood(input, "compression read");
    }
    
    // Finalize
    // ...
    
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt);
    result.peakMemoryBytes = platform::getPeakMemoryUsage();
    
    return result;
}

void ZpaqEngine::decompress(std::istream& input, std::ostream& output) {
    // Initialize zpaq decompressor
    // ...
    
    std::array<char, BUFFER_SIZE> buffer{};
    
    while (true) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        
        if (input.gcount() == 0) {
            if (input.eof()) break;
            ensureStreamGood(input, "decompression read");
        }
        
        // Decompress chunk
        // ...
    }
}

} // namespace mosqueeze
```

---

## Step 3: Update CMake

`src/libmosqueeze/CMakeLists.txt`:

```cmake
add_library(libmosqueeze SHARED
  src/Version.cpp
  src/platform/Platform.cpp
  src/engines/ZstdEngine.cpp
  src/engines/LzmaEngine.cpp
  src/engines/BrotliEngine.cpp
  src/engines/ZpaqEngine.cpp      # NEW
  src/FileTypeDetector.cpp
)

# Add dependency
find_package(zpaq CONFIG REQUIRED)  # or use vcpkg

target_link_libraries(libmosqueeze
  PRIVATE
    zstd::libzstd_shared
    LibLZMA::LibLZMA
    brotli::brotli
    zpaq::zpaq                      # NEW
)
```

---

## Step 4: Add Dependency

`vcpkg.json`:

```json
{
  "dependencies": [
    "zstd",
    "liblzma",
    "brotli",
    "cli11",
    "fmt",
    "spdlog",
    "nlohmann-json",
    "sqlite3",
    "zpaq"                        // NEW
  ]
}
```

---

## Step 5: Create Tests

`tests/unit/ZpaqEngine_test.cpp`:

```cpp
#include <mosqueeze/engines/ZpaqEngine.hpp>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    mosqueeze::ZpaqEngine engine;

    // Roundtrip test
    std::string original = "Test string for zpaq compression. ";
    for (int i = 0; i < 100; ++i) original += original;

    std::istringstream input(original);
    std::ostringstream compressed;

    auto result = engine.compress(input, compressed);
    assert(result.ratio() > 1.0);
    assert(result.compressedBytes < result.originalBytes);

    std::istringstream compressedInput(compressed.str());
    std::ostringstream decompressed;
    engine.decompress(compressedInput, decompressed);

    assert(decompressed.str() == original);

    // Metadata
    auto levels = engine.supportedLevels();
    assert(!levels.empty());
    assert(engine.name() == "zpaq");
    assert(engine.fileExtension() == ".zpaq");

    std::cout << "[PASS] ZpaqEngine roundtrip OK\n";
    return 0;
}
```

`tests/CMakeLists.txt`:

```cmake
# Add to existing tests
add_executable(ZpaqEngine_test
  unit/ZpaqEngine_test.cpp
)

target_link_libraries(ZpaqEngine_test
  PRIVATE
    libmosqueeze
)

add_test(NAME ZpaqEngine_test COMMAND ZpaqEngine_test)
```

---

## Step 6: Update Benchmark Harness

`src/mosqueeze-bench/src/main.cpp`:

```cpp
#include <mosqueeze/engines/ZpaqEngine.hpp>

// In main():
runner.registerEngine(std::make_unique<mosqueeze::ZpaqEngine>());
```

---

## Step 7: Update Documentation

### Wiki Pages

1. Create `docs/wiki/algorithms/zpaq.md`
2. Update `docs/wiki/algorithms/comparison-matrix.md`
3. Update `docs/wiki/decisions/file-type-to-algorithm.md` if needed

### README

Update main `README.md` supported algorithms list.

---

## Step 8: Verify

```bash
# Build
cmake --build build --config Release

# Run tests
cd build && ctest

# Run benchmark
./mosqueeze-bench --corpus corpus/ --algorithms zpaq

# Run CLI
./mosqueeze-cli compress test.txt test.zpaq --algorithm zpaq
```

---

## Checklist

- [ ] Header file created with metadata
- [ ] Implementation with streaming (64KB buffer)
- [ ] CMake updated (source files + dependency)
- [ ] vcpkg.json updated
- [ ] Unit test created
- [ ] Benchmark harness updated
- [ ] Wiki documentation added
- [ ] All tests pass
- [ ] CI passes on all platforms

---

## Common Patterns

### Error Handling

```cpp
void ensureZpaqOk(int code, const char* context) {
    if (code != ZPAQ_OK) {
        throw std::runtime_error(
            std::string(context) + ": zpaq error code " + std::to_string(code));
    }
}
```

### Memory Tracking

```cpp
result.peakMemoryBytes = platform::getPeakMemoryUsage();
```

### Level Clamping

```cpp
int level = opts.level > 0 
    ? std::clamp(opts.level, minLevel(), maxLevel()) 
    : defaultLevel();
```

---

## Related Pages

- [[getting-started]] — Build and test setup
- [[../decisions/streaming-architecture]] — Why streaming
- [[../algorithms/comparison-matrix]] — Compare with existing