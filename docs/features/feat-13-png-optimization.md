# Worker Spec: PNG Re-compression Optimization

**Issue:** [#13](https://github.com/fathorMB/MoSqueeze/issues/13)
**Branch:** `feat/png-optimization`
**Priority:** Medium
**Type:** Feature
**Estimated Effort:** ~500 LOC, 16-20 hours

---

## Overview

Handle PNG images specially: re-compress with aggressive libpng settings to achieve better compression than standard PNG tools. Skip already-compressed lossy formats (JPEG, WebP) as re-compressing won't help and may degrade quality.

---

## Problem Statement

PNG files are already compressed, but can often be compressed further:
- Standard PNG writers use conservative settings for speed
- Better filter selection can reduce file size by 10-30%
- Metadata chunks (EXIF, XMP, text) add overhead
- Palette optimization can reduce truecolor PNGs significantly

JPEG and WebP are lossy — re-compressing won't help and may degrade quality. These should be detected and skipped.

---

## Solution Overview

### Approach: libpng Aggressive Settings

Implement PNG optimization using libpng directly with:
1. Maximum compression level (Z_LEVEL 9)
2. All filter strategies (try each, pick best)
3. Remove non-essential chunks (metadata)
4. Optimize palette if beneficial
5. Strip alpha channel if fully opaque

This avoids external dependency on oxipng (Rust-based) while achieving significant compression gains.

### Alternative Consideration

Future enhancement: shell out to oxipng binary for even better results. oxipng achieves 5-15% better than libpng max settings through:
- Brute-force filter selection
- Zopfli compression (slower but better DEFLATE)
- Advanced heuristics

For now, libpng approach provides good results with native C++ integration.

---

## Implementation Plan

### 1. Create PngOptimizer Class

**File:** `src/libmosqueeze/include/mosqueeze/preprocessors/PngOptimizer.hpp`

```cpp
#pragma once

#include <mosqueeze/Preprocessor.hpp>
#include <mosqueeze/Types.hpp>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <vector>

namespace mosqueeze {

enum class PngOptimizationLevel : uint8_t {
    Fast = 0,      // Quick, good results
    Balanced = 1,  // Medium speed, better results
    Maximum = 2    // Slowest, best results
};

struct PngOptimizeResult {
    size_t originalBytes = 0;
    size_t optimizedBytes = 0;
    size_t removedMetadataBytes = 0;
    bool paletteOptimized = false;
    bool alphaStripped = false;
    std::string filterUsed;
    std::chrono::milliseconds duration{0};
};

class PngOptimizer : public IPreprocessor {
public:
    PreprocessorType type() const override { return PreprocessorType::PngOptimizer; }
    std::string name() const override { return "png-optimizer"; }
    
    bool canProcess(FileType fileType) const override {
        return fileType == FileType::Image_PNG;
    }
    
    PreprocessResult preprocess(
        std::istream& input,
        std::ostream& output,
        FileType fileType) override;
    
    void postprocess(
        std::istream& input,
        std::ostream& output,
        const PreprocessResult& result) override;
    
    double estimatedImprovement(FileType fileType) const override {
        return canProcess(fileType) ? 0.15 : 0.0; // ~15% average
    }
    
    // PNG-specific optimization options
    void setOptimizationLevel(PngOptimizationLevel level) { m_level = level; }
    void setStripMetadata(bool strip) { m_stripMetadata = strip; }
    void setTryAllFilters(bool tryAll) { m_tryAllFilters = tryAll; }
    
private:
    PngOptimizationLevel m_level = PngOptimizationLevel::Balanced;
    bool m_stripMetadata = true;
    bool m_tryAllFilters = true;
    
    // PNG chunk handling
    struct PngChunk {
        uint32_t length;
        std::string type;
        std::vector<uint8_t> data;
        uint32_t crc;
    };
    
    bool readPngChunks(std::istream& input, std::vector<PngChunk>& chunks);
    bool writePngChunks(std::ostream& output, const std::vector<PngChunk>& chunks);
    
    // Optimization functions
    std::vector<uint8_t> optimizeImageData(
        const std::vector<uint8_t>& rawImageData,
        uint32_t width,
        uint32_t height,
        uint8_t bitDepth,
        uint8_t colorType,
        std::string& bestFilter);
    
    std::vector<uint8_t> tryFilterStrategy(
        const std::vector<uint8_t>& imageData,
        int filterType,
        uint32_t width,
        uint32_t height,
        uint8_t bytesPerPixel);
    
    bool isFullyOpaque(const std::vector<uint8_t>& imageData, uint8_t bytesPerPixel);
    
    std::vector<uint8_t> compressZlib(
        const std::vector<uint8_t>& data,
        int compressionLevel);
    
    std::vector<uint8_t> uncompressZlib(
        const std::vector<uint8_t>& compressedData);
};

} // namespace mosqueeze
```

### 2. Implement PngOptimizer

**File:** `src/libmosqueeze/src/preprocessors/PngOptimizer.cpp`

```cpp
#include <mosqueeze/preprocessors/PngOptimizer.hpp>
#include <png.h>
#include <zlib.h>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <chrono>

namespace mosqueeze {

// PNG signature
static constexpr uint8_t PNG_SIGNATURE[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

// Essential PNG chunks
static const std::set<std::string> ESSENTIAL_CHUNKS = {
    "IHDR", "PLTE", "IDAT", "IEND"
};

// Metadata chunks to consider stripping
static const std::set<std::string> METADATA_CHUNKS = {
    "tEXt", "zTXt", "iTXt",  // Text metadata
    "eXIf",                  // EXIF data
    "iCCP", "sRGB",          // Color profile
    "tIME",                  // Timestamp
    "pHYs",                  // Physical dimensions
    "bKGD",                  // Background color
    "tRNS",                  // Transparency
    "sBIT",                  // Significant bits
    "gAMA", "cHRM"           // Gamma and chromaticity
};

PreprocessResult PngOptimizer::preprocess(
    std::istream& input,
    std::ostream& output,
    FileType fileType) {
    
    if (!canProcess(fileType)) {
        throw std::runtime_error("PngOptimizer cannot process this file type");
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Read entire PNG file
    const std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const size_t originalSize = raw.size();
    
    // Validate PNG signature
    if (raw.size() < 8 || std::memcmp(raw.data(), PNG_SIGNATURE, 8) != 0) {
        throw std::runtime_error("Invalid PNG signature");
    }
    
    // Parse PNG chunks
    std::vector<PngChunk> chunks;
    if (!readPngChunks(input, chunks)) {
        throw std::runtime_error("Failed to parse PNG chunks");
    }
    
    // Extract IHDR (header) - required first chunk
    if (chunks.empty() || chunks[0].type != "IHDR") {
        throw std::runtime_error("PNG missing IHDR chunk");
    }
    
    // Parse IHDR
    if (chunks[0].data.size() < 13) {
        throw std::runtime_error("Invalid IHDR chunk size");
    }
    
    uint32_t width = (chunks[0].data[0] << 24) | (chunks[0].data[1] << 16) | 
                     (chunks[0].data[2] << 8) | chunks[0].data[3];
    uint32_t height = (chunks[0].data[4] << 24) | (chunks[0].data[5] << 16) | 
                      (chunks[0].data[6] << 8) | chunks[0].data[7];
    uint8_t bitDepth = chunks[0].data[8];
    uint8_t colorType = chunks[0].data[9];
    uint8_t compression = chunks[0].data[10];
    uint8_t filter = chunks[0].data[11];
    uint8_t interlace = chunks[0].data[12];
    
    // Calculate bytes per pixel for filter strategies
    uint8_t bytesPerPixel = 1;
    if (colorType == 2) bytesPerPixel = 3;           // RGB
    else if (colorType == 4) bytesPerPixel = 2;      // Grayscale + Alpha
    else if (colorType == 6) bytesPerPixel = 4;      // RGBA
    
    // Collect IDAT chunks and decompress
    std::vector<uint8_t> compressedData;
    for (const auto& chunk : chunks) {
        if (chunk.type == "IDAT") {
            compressedData.insert(compressedData.end(), chunk.data.begin(), chunk.data.end());
        }
    }
    
    // Decompress using zlib
    std::vector<uint8_t> rawImageData = uncompressZlib(compressedData);
    if (rawImageData.empty()) {
        throw std::runtime_error("Failed to decompress PNG image data");
    }
    
    // Optimize: try different filter strategies
    std::string bestFilter;
    std::vector<uint8_t> optimizedData;
    
    if (m_tryAllFilters && m_level >= PngOptimizationLevel::Balanced) {
        // Try all 5 filter types and pick best
        size_t bestSize = SIZE_MAX;
        int bestFilterType = 0;
        
        for (int filterType = 0; filterType <= 4; ++filterType) {
            auto filtered = tryFilterStrategy(rawImageData, filterType, width, height, bytesPerPixel);
            auto compressed = compressZlib(filtered, 
                m_level == PngOptimizationLevel::Maximum ? 9 : 6);
            
            if (compressed.size() < bestSize) {
                bestSize = compressed.size();
                bestFilterType = filterType;
                optimizedData = compressed;
            }
        }
        
        bestFilter = std::to_string(bestFilterType);
    } else {
        // Use filter 0 (None) or original
        optimizedData = compressZlib(rawImageData, 
            m_level == PngOptimizationLevel::Maximum ? 9 : 6);
        bestFilter = "0";
    }
    
    // Reconstruct PNG with optimized data
    // ... (build new PNG structure)
    
    // Write optimized PNG
    // ... (writeIHDr, writeIDAT, writeIEND)
    
    auto endTime = std::chrono::steady_clock::now();
    
    PreprocessResult result{};
    result.type = PreprocessorType::PngOptimizer;
    result.originalBytes = originalSize;
    result.processedBytes = /* size of optimized PNG */;
    
    return result;
}

void PngOptimizer::postprocess(
    std::istream& input,
    std::ostream& output,
    const PreprocessResult& result) {
    // PNG optimization is lossless - just copy through
    output << input.rdbuf();
}

bool PngOptimizer::readPngChunks(std::istream& input, std::vector<PngChunk>& chunks) {
    // Skip PNG signature
    input.seekg(8, std::ios::beg);
    
    while (input.good()) {
        PngChunk chunk;
        
        // Read length (4 bytes, big-endian)
        uint32_t length;
        input.read(reinterpret_cast<char*>(&length), 4);
        if (input.gcount() < 4) break;
        chunk.length = ((length >> 24) & 0xFF) | ((length << 8) & 0xFF0000) |
                       ((length >> 8) & 0xFF00) | ((length << 24) & 0xFF000000);
        
        // Read type (4 bytes)
        char type[5] = {0};
        input.read(type, 4);
        chunk.type = type;
        
        // Read data
        chunk.data.resize(chunk.length);
        if (chunk.length > 0) {
            input.read(reinterpret_cast<char*>(chunk.data.data()), chunk.length);
        }
        
        // Read CRC (4 bytes)
        uint32_t crc;
        input.read(reinterpret_cast<char*>(&crc), 4);
        chunk.crc = ((crc >> 24) & 0xFF) | ((crc << 8) & 0xFF0000) |
                    ((crc >> 8) & 0xFF00) | ((crc << 24) & 0xFF000000);
        
        chunks.push_back(chunk);
        
        // IEND is the last chunk
        if (chunk.type == "IEND") break;
    }
    
    return !chunks.empty();
}

std::vector<uint8_t> PngOptimizer::uncompressZlib(const std::vector<uint8_t>& compressedData) {
    std::vector<uint8_t> decompressed;
    decompressed.resize(compressedData.size() * 10); // Estimate
    
    z_stream stream = {};
    stream.next_in = const_cast<uint8_t*>(compressedData.data());
    stream.avail_in = compressedData.size();
    stream.next_out = decompressed.data();
    stream.avail_out = decompressed.size();
    
    if (inflateInit(&stream) != Z_OK) return {};
    
    int result = inflate(&stream, Z_FINISH);
    if (result != Z_STREAM_END) {
        inflateEnd(&stream);
        return {};
    }
    
    decompressed.resize(stream.total_out);
    inflateEnd(&stream);
    
    return decompressed;
}

std::vector<uint8_t> PngOptimizer::compressZlib(
    const std::vector<uint8_t>& data,
    int compressionLevel) {
    
    std::vector<uint8_t> compressed;
    compressed.resize(compressBound(data.size()));
    
    uLongf destLen = compressed.size();
    int result = compress2(
        compressed.data(), &destLen,
        data.data(), data.size(),
        compressionLevel);
    
    if (result != Z_OK) return {};
    
    compressed.resize(destLen);
    return compressed;
}

std::vector<uint8_t> PngOptimizer::tryFilterStrategy(
    const std::vector<uint8_t>& imageData,
    int filterType,
    uint32_t width,
    uint32_t height,
    uint8_t bytesPerPixel) {
    
    std::vector<uint8_t> filtered;
    filtered.reserve(imageData.size());
    
    const size_t rowSize = width * bytesPerPixel;
    size_t offset = 0;
    
    for (uint32_t y = 0; y < height; ++y) {
        // Write filter type for this row
        filtered.push_back(static_cast<uint8_t>(filterType));
        
        const uint8_t* row = imageData.data() + offset + y * (rowSize + 1) + 1;
        
        switch (filterType) {
            case 0: // None
                filtered.insert(filtered.end(), row, row + rowSize);
                break;
            case 1: // Sub
                for (size_t x = 0; x < rowSize; ++x) {
                    uint8_t left = (x >= bytesPerPixel) ? filtered.back() : 0;
                    filtered.push_back(row[x] - left);
                }
                break;
            // ... implement filters 2-4 (Up, Average, Paeth)
            default:
                break;
        }
    }
    
    return filtered;
}

} // namespace mosqueeze
```

### 3. Add PreprocessorType Enum

**File:** `src/libmosqueeze/include/mosqueeze/Preprocessor.hpp`

```cpp
enum class PreprocessorType : uint8_t {
    None = 0,
    JsonCanonicalizer = 1,
    XmlCanonicalizer = 2,
    ImageMetaStripper = 3,
    DictionaryPreprocessor = 4,
    BayerPreprocessor = 5,
    PngOptimizer = 6  // <-- Add this
};
```

### 4. Register in PreprocessorSelector

**File:** `src/libmosqueeze/src/PreprocessorSelector.cpp`

```cpp
#include <mosqueeze/preprocessors/PngOptimizer.hpp>

// In createPreprocessor() switch:
case PreprocessorType::PngOptimizer:
    return std::make_unique<PngOptimizer>();
```

### 5. Update FileType Detection

**File:** `src/libmosqueeze/src/FileTypeDetector.cpp`

Ensure PNG detection returns `FileType::Image_PNG`.

### 6. Add Unit Tests

**File:** `tests/unit/PngOptimizer_test.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <mosqueeze/preprocessors/PngOptimizer.hpp>
#include <sstream>
#include <vector>

TEST_CASE("PngOptimizer detects PNG files", "[png-optimizer]") {
    mosqueeze::PngOptimizer optimizer;
    
    REQUIRE(optimizer.canProcess(mosqueeze::FileType::Image_PNG));
    REQUIRE_FALSE(optimizer.canProcess(mosqueeze::FileType::Image_JPEG));
    REQUIRE_FALSE(optimizer.canProcess(mosqueeze::FileType::Text_Structured));
}

TEST_CASE("PngOptimizer processes valid PNG", "[png-optimizer]") {
    mosqueeze::PngOptimizer optimizer;
    
    // Create a minimal valid PNG (1x1 white pixel)
    std::vector<uint8_t> minimalPng = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, // Signature
        // ... IHDR, IDAT, IEND chunks
    };
    
    std::istringstream input(std::string(minimalPng.begin(), minimalPng.end()));
    std::ostringstream output;
    
    auto result = optimizer.preprocess(input, output, mosqueeze::FileType::Image_PNG);
    
    REQUIRE(result.type == mosqueeze::PreprocessorType::PngOptimizer);
    REQUIRE(result.processedBytes > 0);
    REQUIRE(result.processedBytes <= result.originalBytes);
}

TEST_CASE("PngOptimizer filter strategies", "[png-optimizer]") {
    // Test that different filter strategies produce different results
    // Filter 0 (None) vs Filter 4 (Paeth) should differ
}

TEST_CASE("PngOptimizer preserves image quality", "[png-optimizer]") {
    // Verify that optimized PNG decodes to same pixels as original
}
```

### 7. Add CLI Support

**File:** `src/mosqueeze-cli/src/main.cpp`

```cpp
// Add --optimize-png flag
app.add_flag("--optimize-png", optimizePng, "Optimize PNG images (re-compress with best settings)");

// Add --strip-metadata flag
app.add_flag("--strip-metadata", stripMetadata, "Strip metadata from images (EXIF, XMP, ICC)");
```

### 8. Add to Benchmark Tool

**File:** `src/mosqueeze-bench/src/main.cpp`

```cpp
// Add png-optimizer to preprocessing options
"--preprocess", preprocessMode,
    "Preprocessor mode: auto, none, bayer-raw, image-meta-strip, png-optimizer"
```

---

## Testing Plan

### Unit Tests

1. PNG signature validation
2. Chunk parsing (IHDR, IDAT, IEND)
3. Filter strategy comparison
4. Compression/decompression roundtrip
5. Metadata stripping

### Integration Tests

1. Benchmark PNG corpus (various sizes, color types)
2. Compare against oxipng results
3. Verify image quality preservation (pixel-perfect)
4. Performance at different optimization levels

### Test Files

Create test PNGs with:
- Different color types (grayscale, RGB, RGBA, palette)
- Different bit depths (1, 2, 4, 8, 16)
- Large metadata chunks
- Interlaced vs non-interlaced

---

## Acceptance Criteria

- [ ] PNG files are correctly parsed and reconstructed
- [ ] Filter optimization reduces size vs. original
- [ ] Metadata stripping works correctly
- [ ] All 5 filter strategies implemented (None, Sub, Up, Average, Paeth)
- [ ] Optimization level affects speed/ratio tradeoff
- [ ] JPEG/WebP files are skipped (detected and not processed)
- [ ] Post-processing returns original image data (lossless)
- [ ] Unit tests cover all major code paths
- [ ] Performance: <1 second for 1MB PNG at Balanced level

---

## Dependencies

### Build

- libpng (via vcpkg: `png`)
- zlib (via vcpkg: `zlib`)

### Update vcpkg.json

```json
{
  "dependencies": [
    "png",
    "zlib"
  ]
}
```

### Update CMakeLists.txt

```cmake
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)

target_link_libraries(libmosqueeze PRIVATE PNG::PNG ZLIB::ZLIB)
```

---

## Future Enhancements (Post-MVP)

1. **Oxipng Integration**: Shell out to oxipng binary for better compression
2. **Palette Optimization**: Convert truecolor to palette when beneficial
3. **Alpha Channel Stripping**: Remove alpha channel if fully opaque
4. **APNG Support**: Optimize animated PNGs
5. **Progressive PNG**: Add interlacing support

---

## Notes

- PNG filter types: 0=None, 1=Sub, 2=Up, 3=Average, 4=Paeth
- Best filter varies by image content (photos vs. graphics)
- For photos: usually Average or Paeth
- For graphics: often Sub or None
- Level 9 compression (Z_BEST_COMPRESSION) is slow but achieves best results
- Consider Zopfli for even better DEFLATE (2-3x slower than zlib)

---

## Files to Create/Modify

| File | Action |
|------|--------|
| `src/libmosqueeze/include/mosqueeze/preprocessors/PngOptimizer.hpp` | Create |
| `src/libmosqueeze/src/preprocessors/PngOptimizer.cpp` | Create |
| `src/libmosqueeze/include/mosqueeze/Preprocessor.hpp` | Modify |
| `src/libmosqueeze/src/PreprocessorSelector.cpp` | Modify |
| `src/libmosqueeze/CMakeLists.txt` | Modify |
| `src/libmosqueeze/vcpkg.json` | Modify |
| `tests/unit/PngOptimizer_test.cpp` | Create |
| `tests/CMakeLists.txt` | Modify |