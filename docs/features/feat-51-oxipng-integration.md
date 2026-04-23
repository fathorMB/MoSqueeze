# Worker Spec: oxipng Integration for PNG Optimization

**Issue:** #51 - Feature: oxipng Integration for PNG Optimization  
**Branch:** `feat/oxipng-integration`  
**Priority:** Low  
**Estimated Effort:** 2-3 days  
**Depends on:** #13 (PNG Re-compression Optimization - must be MERGED first)  

---

## Overview

Integrate [oxipng](https://github.com/shssoichiro/oxipng) as an alternative PNG preprocessor for better compression ratios than libpng. oxipng uses Zopfli-style DEFLATE compression and intelligent filter selection to achieve 5-15% better compression than libpng level 9.

---

## Why oxipng?

### libpng Limitations

- Max compression level 9
- Filter selection is basic (tries 5 filters, picks best)
- DEFLATE algorithm is standard zlib
- Typical improvement: 10-30% on uncompressed PNGs

### oxipng Advantages

- **Better filter selection** - Advanced heuristics analyze image patterns
- **Zopfli DEFLATE** - Google's compression algorithm (100x slower, ~5% smaller)
- **PNGOUT compatibility** - Recompresses IDAT chunks optimally
- **Lossless** - Pixel-perfect output guaranteed
- **Typical improvement**: 15-40% on uncompressed PNGs, 5-15% additional over libpng level 9

### Performance Tradeoff

| Engine | Compression | Speed | Use Case |
|--------|-------------|-------|----------|
| libpng (level 6) | Good | Fast | Real-time processing |
| libpng (level 9) | Better | Medium | Batch processing |
| oxipng (level 3) | Best | Slow | Archive/optimize-once |

---

## Architecture

### Engine Selection

```cpp
// src/libmosqueeze/include/mosqueeze/preprocessors/PngOptimizer.hpp
#pragma once

#include "IPreprocessor.hpp"
#include <memory>

namespace mosqueeze {

enum class PngEngine {
    LibPng,   // libpng with max compression
    Oxipng    // oxipng with Zopfli
};

class PngOptimizer : public IPreprocessor {
public:
    explicit PngOptimizer(PngEngine engine = PngEngine::LibPng);
    
    std::expected<std::vector<uint8_t>, PreprocessError>
    preprocess(std::span<const uint8_t> data) override;
    
    std::expected<std::vector<uint8_t>, PreprocessError>
    postprocess(std::span<const uint8_t> data) override;
    
    std::string_view name() const override { return "PngOptimizer"; }
    
    // Configuration
    void setCompressionLevel(int level);  // libpng: 1-9, oxipng: 0-6
    void setStripMetadata(bool strip);    // Remove EXIF, XMP, ICC
    void setFilterSelection(bool all);    // Try all filters vs fast
    
private:
    PngEngine engine_;
    int compressionLevel_;
    bool stripMetadata_;
    bool allFilters_;
};

// Factory for engine-specific implementations
class PngOptimizerFactory {
public:
    static std::unique_ptr<PngOptimizer> create(PngEngine engine);
    static std::vector<PngEngine> availableEngines();
};

} // namespace mosqueeze
```

### libpng Implementation (from #13)

```cpp
// src/libmosqueeze/src/preprocessors/LibPngOptimizer.hpp
// Already implemented in feat/png-optimization
```

### oxipng Implementation

```cpp
// src/libmosqueeze/src/preprocessors/OxipngOptimizer.hpp
#pragma once

#include "PngOptimizer.hpp"
#include <oxipng.h>  // oxipng C API

namespace mosqueeze {

class OxipngOptimizer {
public:
    std::expected<std::vector<uint8_t>, PreprocessError>
    preprocess(std::span<const uint8_t> data);
    
private:
    // Map our settings to oxipng options
    oxipng::Options mapOptions() const;
};

} // namespace mosqueeze
```

```cpp
// src/libmosqueeze/src/preprocessors/OxipngOptimizer.cpp
#include "OxipngOptimizer.hpp"
#include <spdlog/spdlog.h>

namespace mosqueeze {

std::expected<std::vector<uint8_t>, PreprocessError>
OxipngOptimizer::preprocess(std::span<const uint8_t> data) {
    // Validate PNG header
    if (!isPng(data)) {
        return std::unexpected(PreprocessError{
            .code = ErrorCode::InvalidFormat,
            .message = "Input is not a valid PNG"
        });
    }
    
    // oxipng options
    auto opts = oxipng::Options();
    opts.level = static_cast<uint8_t>(compressionLevel_);  // 0-6, maps to Zopfli iterations
    opts.strip = stripMetadata_ ? oxipng::Strip::Safe : oxipng::Strip::None;
    opts.filter = oxipng::Filter::Auto;  // Try all filters
    
    // oxipng operates on files, so we need to use pipe/stdin
    // OR use oxipng library mode with memory buffers
    
    try {
        auto result = oxipng::optimize(data, opts);
        
        spdlog::debug("oxipng: {} -> {} bytes ({:.1f}% reduction)",
            data.size(), result.size(),
            100.0 * (1.0 - result.size() / data.size()));
        
        return result;
        
    } catch (const oxipng::Error& e) {
        return std::unexpected(PreprocessError{
            .code = ErrorCode::ProcessingError,
            .message = fmt::format("oxipng error: {}", e.what())
        });
    }
}

} // namespace mosqueeze
```

---

## oxipng Integration Options

### Option A: oxipng as External Dependency

**Pros:**
- Simple CMake integration
- oxipng team maintains the code

**Cons:**
- Users must install oxipng
- Adds external dependency

```cmake
# CMakeLists.txt
find_package(oxipng QUIET)
if(oxipng_FOUND)
    target_compile_definitions(mosqueeze PRIVATE OXIPNG_AVAILABLE=1)
    target_link_libraries(mosqueeze PRIVATE oxipng::oxipng)
else()
    message(STATUS "oxipng not found - using libpng only")
endif()
```

### Option B: Build oxipng from Source

**Pros:**
- Self-contained build
- No system dependency

**Cons:**
- Longer build time
- Rust toolchain required (oxipng is Rust)

```cmake
# CMakeLists.txt
option(BUILD_OXIPNG "Build oxipng from source" OFF)
if(BUILD_OXIPNG)
    include(ExternalProject)
    ExternalProject_Add(oxipng
        GIT_REPOSITORY https://github.com/shssoichiro/oxipng
        GIT_TAG v9.1.0
        CONFIGURE_COMMAND cargo build --release
        BUILD_IN_SOURCE ON
        INSTALL_COMMAND ""
    )
    # Build Rust library for C++ integration
endif()
```

### Option C: CLI Wrapper (Recommended for MVP)

**Pros:**
- Simplest implementation
- Works with system oxipng install

**Cons:**
- Process spawn overhead
- Requires oxipng in PATH

```cpp
// src/libmosqueeze/src/preprocessors/OxipngOptimizer.cpp
std::expected<std::vector<uint8_t>, PreprocessError>
OxipngOptimizer::preprocess(std::span<const uint8_t> data) {
    // Write to temp file
    auto tempFile = writeTempFile(data, ".png");
    
    // Call oxipng CLI
    auto result = subprocess::call({
        "oxipng",
        "-o", std::to_string(compressionLevel_),
        "--strip", "safe",
        tempFile
    });
    
    if (result.exit_code != 0) {
        return std::unexpected(PreprocessError{
            .code = ErrorCode::ProcessingError,
            .message = fmt::format("oxipng CLI failed: {}", result.stderr)
        });
    }
    
    // Read result
    return readFile(tempFile);
}
```

### Recommended Approach

Start with **Option A (external dependency)** for CMake integration:

1. Add `find_package(oxipng)` to CMake
2. Guard oxipng code with `#ifdef OXIPNG_AVAILABLE`
3. Fall back to libpng if oxipng not found
4. Document oxipng installation in README

---

## CLI Integration

### --png-engine Flag

```bash
# Use libpng (default, fastest)
mosqueeze-bench -d ./images/ -a zstd --preprocess png --png-engine libpng

# Use oxipng (best compression, slower)
mosqueeze-bench -d ./images/ -a zstd --preprocess png --png-engine oxipng

# Specify compression level
mosqueeze-bench -d ./images/ -a zstd --preprocess png --png-engine oxipng --png-level 4
```

### Auto-detection

If oxipng is unavailable, fall back to libpng automatically:

```cpp
if (engine == PngEngine::Oxipng && !PngOptimizerFactory::isAvailable(PngEngine::Oxipng)) {
    spdlog::warn("oxipng not available, falling back to libpng");
    engine = PngEngine::LibPng;
}
```

---

## Compression Level Mapping

| oxipng Level | libpng Equivalent | Description |
|--------------|-------------------|-------------|
| 0 | N/A | No compression, just filter + metadata |
| 1-2 | N/A | Fast compression, low iterations |
| 3 | ~level 6 | Balanced (recommended) |
| 4 | N/A | More Zopfli iterations |
| 5-6 | ~level 9+ | Maximum compression, slowest |

---

## Benchmark Comparison

Expected results on PNG test corpus:

| File | Original | libpng L9 | oxipng L3 | oxipng L6 |
|------|----------|-----------|-----------|-----------|
| photos/landscape.png | 5.2 MB | 4.8 MB (8%) | 4.5 MB (13%) | 4.4 MB (15%) |
| screenshots/ui.png | 1.8 MB | 1.5 MB (17%) | 1.3 MB (28%) | 1.2 MB (33%) |
| graphics/logo.png | 320 KB | 280 KB (13%) | 250 KB (22%) | 240 KB (25%) |

**Key insight:** oxipng excels on screenshots and graphics (high color coherence, flat areas). Photos show less improvement.

---

## Implementation Steps

### Phase 1: CMake Integration (0.5 day)
- [ ] Add `find_package(oxipng)` to CMakeLists.txt
- [ ] Add `OXIPNG_AVAILABLE` compile definition
- [ ] Update vcpkg/Conan dependencies
- [ ] Test build on Linux/macOS/Windows

### Phase 2: OxipngOptimizer Class (1 day)
- [ ] Create `OxipngOptimizer.hpp` and `.cpp`
- [ ] Implement `preprocess()` with oxipng library
- [ ] Add error handling for missing oxipng
- [ ] Write unit tests

### Phase 3: PngOptimizerFactory (0.5 day)
- [ ] Create factory for engine selection
- [ ] Add `availableEngines()` method
- [ ] Add fallback logic for missing oxipng
- [ ] Add engine selection to CLI

### Phase 4: CLI Updates (0.5 day)
- [ ] Add `--png-engine {libpng,oxipng}` flag
- [ ] Add `--png-level N` flag
- [ ] Add `--strip-metadata` flag
- [ ] Update help text

### Phase 5: Testing (0.5 day)
- [ ] Create PNG test fixtures
- [ ] Benchmark libpng vs oxipng on same corpus
- [ ] Test fallback when oxipng unavailable
- [ ] Test on all platforms

---

## Dependencies

### System Requirements

```bash
# Ubuntu/Debian
apt install oxipng

# macOS
brew install oxipng

# Windows (vcpkg)
vcpkg install oxipng

# Or from source (Rust required)
cargo install oxipng
```

### CMake Integration

```cmake
# CMakeLists.txt
option(MOSQUEEZE_USE_OXIPNG "Use oxipng for PNG optimization" ON)

if(MOSQUEEZE_USE_OXIPNG)
    find_package(oxipng CONFIG QUIET)
    if(oxipng_FOUND)
        message(STATUS "Found oxipng ${oxipng_VERSION}")
        target_compile_definitions(mosqueeze PRIVATE OXIPNG_AVAILABLE=1)
        target_link_libraries(mosqueeze PRIVATE oxipng::oxipng)
    else()
        message(WARNING "oxipng not found - PNG optimization will use libpng only")
    endif()
endif()
```

---

## Test Cases

```cpp
TEST_CASE("PngOptimizerFactory: Available Engines") {
    auto engines = PngOptimizerFactory::availableEngines();
    REQUIRE(std::find(engines.begin(), engines.end(), PngEngine::LibPng) != engines.end());
    // oxipng may or may not be available depending on build
}

TEST_CASE("PngOptimizer: libpng level 9 compression") {
    auto optimizer = PngOptimizerFactory::create(PngEngine::LibPng);
    optimizer->setCompressionLevel(9);
    
    auto png = loadPngFile("test.png");
    auto result = optimizer->preprocess(png);
    
    REQUIRE(result.has_value());
    REQUIRE(result->size() < png.size());
    
    // Verify round-trip
    auto restored = optimizer->postprocess(*result);
    REQUIRE(restored.has_value());
    REQUIRE(*restored == png);
}

TEST_CASE("PngOptimizer: oxipng compression") {
    if (!PngOptimizerFactory::isAvailable(PngEngine::Oxipng)) {
        SKIP("oxipng not available");
    }
    
    auto optimizer = PngOptimizerFactory::create(PngEngine::Oxipng);
    optimizer->setCompressionLevel(3);
    
    auto png = loadPngFile("test.png");
    auto result = optimizer->preprocess(png);
    
    REQUIRE(result.has_value());
    REQUIRE(result->size() < png.size());
    
    // oxipng should be better than libpng
    auto libpng = PngOptimizerFactory::create(PngEngine::LibPng);
    auto libpngResult = libpng->preprocess(png);
    
    // oxipng result should be <= libpng result
    REQUIRE(result->size() <= libpngResult->size());
}

TEST_CASE("PngOptimizer: fallback to libpng") {
    auto optimizer = PngOptimizerFactory::create(PngEngine::Oxipng);
    // If oxipng unavailable, should fall back to libpng
    REQUIRE(optimizer != nullptr);
    REQUIRE(optimizer->name() == "PngOptimizer");
}
```

---

## CI/CD Updates

```yaml
# .github/workflows/ci.yml
- name: Install oxipng (Linux)
  if: runner.os == 'Linux'
  run: |
    wget https://github.com/shssoichiro/oxipng/releases/download/v9.1.0/oxipng-9.1.0-x86_64-unknown-linux-musl.tar.gz
    tar xzf oxipng-*.tar.gz
    sudo mv oxipng-*/oxipng /usr/local/bin/

- name: Install oxipng (macOS)
  if: runner.os == 'macOS'
  run: brew install oxipng

- name: Install oxipng (Windows)
  if: runner.os == 'Windows'
  run: |
    Invoke-WebRequest -Uri https://github.com/shssoichiro/oxipng/releases/download/v9.1.0/oxipng-9.1.0-x86_64-pc-windows-msvc.zip -OutFile oxipng.zip
    Expand-Archive oxipng.zip -DestinationPath .
    Move-Item oxipng.exe -Destination "C:\Program Files\"
```

---

## Documentation

### README.md Addition

```markdown
## PNG Optimization Engines

MoSqueeze supports multiple PNG optimization engines:

### libpng (default)
- **Pros:** Fast, widely available, standard DEFLATE
- **Cons:** Lower compression than oxipng
- **Use for:** Real-time processing, batch jobs where speed matters
- **Levels:** 1-9 (default: 9)

### oxipng
- **Pros:** Best compression, Zopfli algorithm, smart filtering
- **Cons:** Slower (~5-10x), requires installation
- **Use for:** Archive optimization, when compression matters most
- **Levels:** 0-6 (default: 3)

### Installation

```bash
# Ubuntu/Debian
sudo apt install oxipng

# macOS
brew install oxipng

# Windows (vcpkg)
vcpkg install oxipng

# From source (Rust required)
cargo install oxipng
```

### Usage

```bash
# Use libpng (default)
mosqueeze-bench -d ./images/ -a zstd --preprocess png

# Use oxipng
mosqueeze-bench -d ./images/ -a zstd --preprocess png --png-engine oxipng

# Specify compression level
mosqueeze-bench -d ./images/ -a zstd --preprocess png --png-engine oxipng --png-level 4

# Strip metadata (EXIF, XMP, ICC)
mosqueeze-bench -d ./images/ -a zstd --preprocess png --strip-metadata
```

### Benchmark Comparison

| Engine | Level | Time | Compression |
|--------|-------|------|-------------|
| libpng | 6 | 0.5s | 12% |
| libpng | 9 | 1.2s | 15% |
| oxipng | 3 | 2.5s | 20% |
| oxipng | 6 | 8.0s | 22% |
```

---

## Acceptance Criteria

- [ ] oxipng integration compiles conditionally
- [ ] Graceful fallback to libpng when oxipng unavailable
- [ ] `--png-engine {libpng,oxipng}` CLI flag working
- [ ] `--png-level N` CLI flag working
- [ ] oxipng achieves better compression than libpng (5-15% improvement)
- [ ] Unit tests pass with and without oxipng
- [ ] CI/CD installs oxipng for testing
- [ ] Documentation updated with engine selection and benchmark comparison
- [ ] README updated with installation instructions

---

## Related Issues

- #13: PNG Re-compression Optimization (base implementation, must merge first)
- #52: Preprocessor Unit Test Suite (tests needed)

---

## Future Enhancements

- **Parallel oxipng** - Run multiple oxipng instances in parallel
- **oxipng cache** - Skip re-optimization of already-optimal PNGs
- **Hybrid mode** - Try libpng first, then oxipng if savings > threshold
- **PNGOUT integration** - Third engine option for maximum compression

---

## Risks

1. **Build complexity** - Rust toolchain may complicate Windows builds
2. **Performance** - oxipng is slower, need to communicate this clearly
3. **Availability** - Not all platforms have oxipng packages
4. **API changes** - oxipng C API may change (use versioned dependency)

---

## Notes

- This feature is lower priority than #13 (base PNG optimization)
- Wait for #13 to merge before starting this work
- Consider oxipng as "premium" feature, libpng as "standard"
- oxipng is ideal for archive optimization, libpng for real-time processing