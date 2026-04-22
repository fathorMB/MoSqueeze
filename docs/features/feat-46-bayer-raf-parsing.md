# Worker Spec: Fix BayerPreprocessor to Parse RAF Format

**Issue:** [#46](https://github.com/fathorMB/MoSqueeze/issues/46)
**Branch:** `fix/bayer-preprocessor-raf-parsing`
**Priority:** High
**Type:** Bug Fix
**Estimated Effort:** ~400 LOC, 12-16 hours

---

## Problem

The `BayerPreprocessor` currently transforms the **entire RAF file** (header + metadata + image data), when it should only transform the **Bayer pixel data**.

### Evidence from Benchmark

| Metric | Without Preprocessing | With BayerPreprocessor |
|--------|----------------------|------------------------|
| ZPAQ ratio | ~1.039x | ~1.016x |
| Average ratio | ~1.02-1.04x | ~1.014-1.016x |
| Savings | ~3.5% | ~1.5% |

**Compression is WORSE after preprocessing** because:
1. Header and metadata are transformed (increases entropy)
2. File structure is corrupted (breaks decodability)
3. Bayer transform only effective on raw pixel data

### Root Cause

```cpp
// Current BayerPreprocessor::preprocess()
const std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
const size_t words = originalSize / 2;
// Transforms ENTIRE file, including headers!
for (size_t i = 0; i < words; ++i) {
    transformed[i] = static_cast<uint8_t>(raw[(i * 2)]);
    transformed[words + i] = static_cast<uint8_t>(raw[(i * 2) + 1]);
}
```

---

## Solution Overview

Modify `BayerPreprocessor` to:
1. **Parse RAF header** to find RAW image data offset and size
2. **Split file into regions**: header, metadata, image data
3. **Apply Bayer transform only to image data**
4. **Reconstruct file** with unchanged header + metadata + transformed pixels

---

## FujiFilm RAF Format Reference

### File Structure

```
Offset 0x00:  Magic bytes "FUJIFILM" (8 bytes)
Offset 0x08:  Version string (4 bytes, e.g., "0100")
Offset 0x0C:  Camera ID (8 bytes, e.g., "X-T5    ")
Offset 0x14:  Camera string (32 bytes)
Offset 0x34:  Directory offset (4 bytes, LE)
...           (padding/reserved)
Offset NNN:   IFD (Image File Directory) - TIFF structure
              - Resolution, dimensions, bit depth
              - Offset to RAW image data
              - Bayer pattern info
Offset MMM:   JPEG thumbnail (optional)
Offset PPP:   RAW image data (Bayer pattern pixels)
```

### Key Fields to Extract

1. **Directory offset** (at 0x34) → Points to IFD
2. **IFD entries** → Find `StripOffsets` or `RAWImageOffset`
3. **Image size** → `ImageWidth`, `ImageLength`, `BitsPerSample`
4. **Bayer pattern** → CFA pattern from metadata

### Minimal Parsing Strategy

For FujiFilm RAF files:
1. Read magic bytes to confirm "FUJIFILM"
2. Read directory offset at byte 0x34 (little-endian uint32)
3. Parse IFD entries starting at directory offset
4. Find entry with tag `StripOffsets` (273) or custom RAW offset
5. Calculate image data size from dimensions × bits per pixel
6. Apply transform to image data region only

---

## Implementation Plan

### 1. Add RAF Parser Module

**File:** `src/libmosqueeze/src/preprocessors/RafParser.hpp`

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mosqueeze {

struct RafMetadata {
    std::string cameraId;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bitsPerSample = 16;
    uint32_t rawImageOffset = 0;
    uint32_t rawImageSize = 0;
    bool valid = false;
};

class RafParser {
public:
    static RafMetadata parse(const std::vector<uint8_t>& data);
    static bool isRafFile(const std::vector<uint8_t>& data);
    
private:
    static bool validateMagicBytes(const std::vector<uint8_t>& data);
    static uint32_t readDirectoryOffset(const std::vector<uint8_t>& data);
    static bool parseIFD(const std::vector<uint8_t>& data, uint32_t offset, RafMetadata& meta);
    static uint32_t readLE32(const std::vector<uint8_t>& data, size_t offset);
    static uint16_t readLE16(const std::vector<uint8_t>& data, size_t offset);
};

} // namespace mosqueeze
```

### 2. Implement RAF Parser

**File:** `src/libmosqueeze/src/preprocessors/RafParser.cpp`

```cpp
#include "RafParser.hpp"
#include <stdexcept>

namespace mosqueeze {

bool RafParser::isRafFile(const std::vector<uint8_t>& data) {
    return data.size() >= 8 && 
           data[0] == 'F' && data[1] == 'U' && 
           data[2] == 'J' && data[3] == 'I' &&
           data[4] == 'F' && data[5] == 'I' && 
           data[6] == 'L' && data[7] == 'M';
}

uint32_t RafParser::readLE32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

uint16_t RafParser::readLE16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 2 > data.size()) return 0;
    return static_cast<uint16_t>(data[offset]) |
           (static_cast<uint16_t>(data[offset + 1]) << 8);
}

bool RafParser::validateMagicBytes(const std::vector<uint8_t>& data) {
    return isRafFile(data);
}

uint32_t RafParser::readDirectoryOffset(const std::vector<uint8_t>& data) {
    // RAF stores IFD offset at byte 0x34 (little-endian)
    return readLE32(data, 0x34);
}

bool RafParser::parseIFD(const std::vector<uint8_t>& data, uint32_t offset, RafMetadata& meta) {
    if (offset + 2 > data.size()) return false;
    
    uint16_t entryCount = readLE16(data, offset);
    size_t entryOffset = offset + 2;
    
    // TIFF IFD entry structure: tag (2) + type (2) + count (4) + value/offset (4)
    constexpr size_t ENTRY_SIZE = 12;
    
    for (uint16_t i = 0; i < entryCount && entryOffset + ENTRY_SIZE <= data.size(); ++i) {
        uint16_t tag = readLE16(data, entryOffset);
        uint16_t type = readLE16(data, entryOffset + 2);
        uint32_t count = readLE32(data, entryOffset + 4);
        uint32_t value = readLE32(data, entryOffset + 8);
        
        // Standard TIFF tags
        constexpr uint16_t TAG_IMAGE_WIDTH = 256;
        constexpr uint16_t TAG_IMAGE_LENGTH = 257;
        constexpr uint16_t TAG_BITS_PER_SAMPLE = 258;
        constexpr uint16_t TAG_STRIP_OFFSETS = 273;
        
        // FujiFilm specific tags (may vary)
        // Some RAF use custom offsets for RAW data
        
        switch (tag) {
            case TAG_IMAGE_WIDTH:
                meta.width = value;
                break;
            case TAG_IMAGE_LENGTH:
                meta.height = value;
                break;
            case TAG_BITS_PER_SAMPLE:
                meta.bitsPerSample = static_cast<uint16_t>(value); // May be array
                break;
            case TAG_STRIP_OFFSETS:
                meta.rawImageOffset = value;
                break;
        }
        
        entryOffset += ENTRY_SIZE;
    }
    
    // Calculate expected image size
    if (meta.width > 0 && meta.height > 0 && meta.bitsPerSample > 0) {
        meta.rawImageSize = (meta.width * meta.height * meta.bitsPerSample) / 8;
    }
    
    return meta.width > 0 && meta.height > 0 && meta.rawImageOffset > 0;
}

RafMetadata RafParser::parse(const std::vector<uint8_t>& data) {
    RafMetadata meta;
    
    if (!validateMagicBytes(data)) {
        meta.valid = false;
        return meta;
    }
    
    // Extract camera ID (bytes 12-19)
    if (data.size() >= 20) {
        meta.cameraId = std::string(reinterpret_cast<const char*>(&data[12]), 8);
    }
    
    uint32_t dirOffset = readDirectoryOffset(data);
    if (dirOffset == 0 || dirOffset >= data.size()) {
        meta.valid = false;
        return meta;
    }
    
    if (!parseIFD(data, dirOffset, meta)) {
        meta.valid = false;
        return meta;
    }
    
    meta.valid = true;
    return meta;
}

} // namespace mosqueeze
```

### 3. Modify BayerPreprocessor

**File:** `src/libmosqueeze/src/preprocessors/BayerPreprocessor.cpp`

```cpp
#include <mosqueeze/preprocessors/BayerPreprocessor.hpp>
#include "RafParser.hpp"
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace mosqueeze {

// ... existing writeU32LE, readU32LE functions ...

PreprocessResult BayerPreprocessor::preprocess(
    std::istream& input,
    std::ostream& output,
    FileType fileType) {
    if (!canProcess(fileType)) {
        throw std::runtime_error("BayerPreprocessor cannot process this file type");
    }

    // Read entire file
    const std::string raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const size_t originalSize = raw.size();
    
    std::vector<uint8_t> fileData(raw.begin(), raw.end());
    
    // Try to parse as RAF file
    RafMetadata meta = RafParser::parse(fileData);
    
    PreprocessResult result{};
    result.type = PreprocessorType::BayerPreprocessor;
    result.originalBytes = originalSize;
    
    if (meta.valid && meta.rawImageOffset > 0 && 
        meta.rawImageOffset + meta.rawImageSize <= originalSize) {
        // === RAF file with valid structure ===
        // Copy header and metadata unchanged
        output.write(raw.data(), meta.rawImageOffset);
        
        // Transform only the RAW image data
        const size_t imageDataOffset = meta.rawImageOffset;
        const size_t imageSize = meta.rawImageSize;
        const size_t words = imageSize / 2;
        
        std::vector<uint8_t> transformed;
        transformed.resize(imageSize);
        
        for (size_t i = 0; i < words; ++i) {
            transformed[i] = static_cast<uint8_t>(fileData[imageDataOffset + (i * 2)]);
            transformed[words + i] = static_cast<uint8_t>(fileData[imageDataOffset + (i * 2) + 1]);
        }
        if ((imageSize % 2) != 0U) {
            transformed[words * 2] = static_cast<uint8_t>(fileData[imageDataOffset + words * 2]);
        }
        
        output.write(reinterpret_cast<const char*>(transformed.data()), 
                     static_cast<std::streamsize>(transformed.size()));
        
        // Copy any trailing data (thumbnails, etc.)
        if (imageDataOffset + imageSize < originalSize) {
            output.write(raw.data() + imageDataOffset + imageSize, 
                        originalSize - imageDataOffset - imageSize);
        }
        
        result.processedBytes = originalSize; // Same size, but transformed content
        
        // Store metadata for reconstruction
        result.metadata.reserve(13);
        result.metadata.push_back(2); // metadata version 2 (with offset info)
        writeU32LE(result.metadata, static_cast<uint32_t>(originalSize));
        writeU32LE(result.metadata, meta.rawImageOffset);
        writeU32LE(result.metadata, meta.rawImageSize);
    } else {
        // === Fallback: Cannot parse RAF structure ===
        // Log warning and skip transformation (preserve file as-is)
        // This ensures we don't corrupt files we don't understand
        
        // Copy file unchanged
        output.write(raw.data(), originalSize);
        result.processedBytes = originalSize;
        
        result.metadata.push_back(0); // metadata version 0 = no transformation
    }
    
    return result;
}

void BayerPreprocessor::postprocess(
    std::istream& input,
    std::ostream& output,
    const PreprocessResult& result) {
    
    std::vector<uint8_t> processed((std::istreambuf_iterator<char>(input)), 
                                    std::istreambuf_iterator<char>());
    
    if (result.metadata.size() < 1) {
        output.write(reinterpret_cast<const char*>(processed.data()), 
                    static_cast<std::streamsize>(processed.size()));
        return;
    }
    
    uint8_t version = result.metadata[0];
    
    if (version == 0) {
        // No transformation was applied, output as-is
        output.write(reinterpret_cast<const char*>(processed.data()), 
                    static_cast<std::streamsize>(processed.size()));
        return;
    }
    
    if (version == 2 && result.metadata.size() >= 13) {
        // Reconstruct from transformed format
        uint32_t originalSize = 0, imageOffset = 0, imageSize = 0;
        if (!readU32LE(result.metadata, 1, originalSize) ||
            !readU32LE(result.metadata, 5, imageOffset) ||
            !readU32LE(result.metadata, 9, imageSize)) {
            output.write(reinterpret_cast<const char*>(processed.data()), 
                        static_cast<std::streamsize>(processed.size()));
            return;
        }
        
        // Verify sizes
        if (processed.size() != originalSize) {
            output.write(reinterpret_cast<const char*>(processed.data()), 
                        static_cast<std::streamsize>(processed.size()));
            return;
        }
        
        // Copy header unchanged
        output.write(reinterpret_cast<const char*>(processed.data()), imageOffset);
        
        // Reverse transform image data
        const size_t imageDataOffset = imageOffset;
        const size_t words = imageSize / 2;
        
        std::vector<uint8_t> restored;
        restored.resize(imageSize);
        
        for (size_t i = 0; i < words; ++i) {
            restored[i * 2] = processed[imageDataOffset + i];
            restored[(i * 2) + 1] = processed[imageDataOffset + words + i];
        }
        if ((imageSize % 2) != 0U) {
            restored[words * 2] = processed[imageDataOffset + words * 2];
        }
        
        output.write(reinterpret_cast<const char*>(restored.data()), 
                    static_cast<std::streamsize>(restored.size()));
        
        // Copy trailing data unchanged
        if (imageOffset + imageSize < processed.size()) {
            output.write(reinterpret_cast<const char*>(processed.data()) + imageOffset + imageSize,
                        processed.size() - imageOffset - imageSize);
        }
        
        return;
    }
    
    // Legacy version 1 (original implementation) - full file transform
    if (version == 1 && result.metadata.size() >= 5) {
        uint32_t originalSize32 = 0;
        if (!readU32LE(result.metadata, 1, originalSize32)) {
            output.write(reinterpret_cast<const char*>(processed.data()), 
                        static_cast<std::streamsize>(processed.size()));
            return;
        }
        
        const size_t originalSize = static_cast<size_t>(originalSize32);
        if (processed.size() != originalSize) {
            output.write(reinterpret_cast<const char*>(processed.data()), 
                        static_cast<std::streamsize>(processed.size()));
            return;
        }
        
        const size_t words = originalSize / 2;
        std::vector<uint8_t> restored;
        restored.resize(originalSize);
        
        for (size_t i = 0; i < words; ++i) {
            restored[i * 2] = processed[i];
            restored[(i * 2) + 1] = processed[words + i];
        }
        if ((originalSize % 2) != 0U) {
            restored[words * 2] = processed[words * 2];
        }
        
        output.write(reinterpret_cast<const char*>(restored.data()), 
                    static_cast<std::streamsize>(restored.size()));
    }
}

// ... rest of BayerPreprocessor implementation unchanged ...

} // namespace mosqueeze
```

### 4. Add Unit Tests

**File:** `tests/unit/RafParser_test.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "RafParser.hpp"
#include <vector>

TEST_CASE("RafParser::isRafFile validates magic bytes", "[rafparser]") {
    SECTION("Valid RAF magic bytes") {
        std::vector<uint8_t> data = {'F', 'U', 'J', 'I', 'F', 'I', 'L', 'M'};
        REQUIRE(mosqueeze::RafParser::isRafFile(data));
    }
    
    SECTION("Invalid magic bytes") {
        std::vector<uint8_t> data = {'I', 'I', '*', 0}; // TIFF magic
        REQUIRE_FALSE(mosqueeze::RafParser::isRafFile(data));
    }
    
    SECTION("Too short") {
        std::vector<uint8_t> data = {'F', 'U', 'J'};
        REQUIRE_FALSE(mosqueeze::RafParser::isRafFile(data));
    }
}

TEST_CASE("RafParser::parse extracts metadata", "[rafparser]") {
    // Create minimal valid RAF structure for testing
    // This would need real RAF file fixtures
}

TEST_CASE("BayerPreprocessor preserves RAF header", "[bayerpreprocessor]") {
    // Test that header is NOT transformed
}

TEST_CASE("BayerPreprocessor transforms only image data", "[bayerpreprocessor]") {
    // Test that only image region is transformed
}
```

### 5. Update CMakeLists.txt

**File:** `src/libmosqueeze/CMakeLists.txt`

Add:
```cmake
preprocessors/RafParser.cpp
preprocessors/RafParser.hpp
```

---

## Verification Plan

### 1. Unit Tests

```bash
cd build
cmake --build . --config Release
ctest --test-dir tests -R "RafParser|BayerPreprocessor" -V
```

### 2. Integration Test

```bash
# Test with real RAF file
./mosqueeze-bench -f "test.raf" -a zstd --preprocess bayer-raw --dry-run --verbose
```

### 3. Compression Test

Before fix:
```
ratio ~1.015x (WORSE than no preprocessing)
```

After fix:
```
ratio should improve (entropy reduction in Bayer data)
```

### 4. Roundtrip Test

```bash
# Compress with preprocessing
./mosqueeze-cli compress input.raf output.msq --preprocess bayer-raw -a zstd

# Decompress
./mosqueeze-cli decompress output.msq restored.raf

# Verify files match
md5sum input.raf restored.raf
```

---

## Acceptance Criteria

- [ ] `RafParser` correctly identifies RAF files by magic bytes
- [ ] `RafParser` extracts image offset, dimensions, and size from IFD
- [ ] `BayerPreprocessor` leaves header and metadata UNCHANGED
- [ ] `BayerPreprocessor` transforms only RAW image data region
- [ ] Roundtrip (compress + decompress) produces identical file
- [ ] Unit tests pass for RAF parsing
- [ ] Integration test shows compression improvement (not degradation)
- [ ] Fallback mode for unparseable files (no transformation, no corruption)

---

## Edge Cases

1. **Non-RAF files**: Return original file unchanged (metadata version 0)
2. **Truncated RAF**: Return original file unchanged
3. **Missing IFD entries**: Return original file unchanged
4. **Corrupt offset/size**: Return original file unchanged
5. **Large files**: Stream processing (don't load entire file in memory for transformation)

---

## Performance Considerations

- RAF metadata parsing adds ~1-5ms overhead
- Bayer transform still O(n) on image data
- Memory: additional buffer for image data transformation
- Can optimize by streaming transform directly to output

---

## Files to Create/Modify

| File | Action |
|------|--------|
| `src/libmosqueeze/src/preprocessors/RafParser.hpp` | Create |
| `src/libmosqueeze/src/preprocessors/RafParser.cpp` | Create |
| `src/libmosqueeze/src/preprocessors/BayerPreprocessor.cpp` | Modify |
| `src/libmosqueeze/CMakeLists.txt` | Modify |
| `tests/unit/RafParser_test.cpp` | Create |
| `tests/unit/BayerPreprocessor_test.cpp` | Modify |
| `tests/fixtures/` | Add test RAF files |

---

## Notes

- RAF format is based on TIFF but with FujiFilm-specific extensions
- May need to handle multiple RAF versions (different cameras)
- Some RAF files embed JPEG thumbnails - these should be preserved
- Bayer pattern (RGGB, GRBG, etc.) detection not required for transform, but useful for metadata