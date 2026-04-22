# Worker Spec: RAW Bayer Preprocessor for Improved Compression

**Issue:** #36  
**Branch:** `feat/benchmark-improvements`  
**Priority:** Low  
**Type:** Enhancement (Optimization)

---

## Problem Statement

RAW image files contain uncompressed Bayer pattern sensor data. The RGGB (or similar) pattern has predictable spatial structure that generic compressors don't exploit efficiently.

### Bayer Pattern Background

Camera sensors use a Bayer filter mosaic:

```
R  G1  R  G1  R  G1 ...
G2 B   G2 B   G2 B ...
R  G1  R  G1  R  G1 ...
G2 B   G2 B   G2 B ...
```

Each cell is a 2×2 block with pattern R-G1 / G2-B. This regular structure offers compression opportunities:

1. **Predictable pattern:** Color channels can be separated
2. **Spatial correlation:** Adjacent pixels of same color have similar values
3. **Delta encoding:** Differences between neighbors are smaller than absolute values
4. **Bit packing:** 14-bit values often stored as 16-bit with padding

## Potential Compression Improvement

| Compressor | Current (raw file) | With Bayer Preproc | Improvement |
|------------|-------------------|-------------------|-------------|
| Zstd | 1.05-1.10x | 1.20-1.30x | +10-20% |
| LZMA | 1.08-1.12x | 1.25-1.35x | +12-20% |
| ZPAQ | 1.10-1.15x | 1.30-1.45x | +15-25% |

---

## Bayer Pattern Variations

| Pattern | Cameras | Arrangement |
|---------|----------|--------------|
| RGGB | Most common | Canon, Nikon, Sony |
| GRBG | Some Nikons | Rotated 90° |
| GBRG | Some cameras | Different G positions |
| BGGR | Fuji X-Trans variants | Blue-first |

The preprocessor must detect the pattern from CFA (Color Filter Array) metadata.

---

## Implementation Plan

### Phase 1: Research & Design

Before implementation, research existing approaches:

1. **JPEG XL modular mode** — Uses MA-like context modeling for Bayer
2. **FLIF** — Uses MANIAC trees with near-lossless preprocessing
3. **libraw** — Reference for Bayer pattern extraction
4. **DNG SDK** — Adobe's reference for RAW handling

Key decisions:
- Pattern detection from metadata vs heuristic detection
- Bit depth handling (12-bit, 14-bit, 16-bit)
- Reversibility requirements (lossless reconstruction)
- Performance requirements (streaming vs full-buffer)

### Phase 2: BayerPreprocessor Interface

**File:** `src/libmosqueeze/include/mosqueeze/preprocessors/BayerPreprocessor.hpp`

```cpp
#pragma once

#include <mosqueeze/Preprocessor.hpp>
#include <mosqueeze/FileTypeDetector.hpp>
#include <cstdint>
#include <vector>
#include <optional>
#include <array>

namespace mosqueeze {

/**
 * @brief Bayer filter pattern types
 */
enum class BayerPattern {
    RGGB,   // Most common: R-G-G-B
    GRBG,   // Rotated: G-R-B-G
    GBRG,   // Alternative: G-B-R-G
    BGGR,   // Inverted: B-G-G-R
    Unknown // Pattern could not be determined
};

/**
 * @brief RAW image metadata extracted from TIFF/DNG
 */
struct RawMetadata {
    uint32_t width;
    uint32_t height;
    uint16_t bitsPerSample;        // Typically 12, 14, or 16
    uint16_t samplesPerPixel;      // Usually 1 for RAW
    BayerPattern pattern;
    uint16_t blackLevel;           // Sensor black level
    uint16_t whiteLevel;           // Sensor white level
    std::array<uint16_t, 4> cfaRepeatPattern; // CFA dimensions
};

/**
 * @brief Preprocessor for RAW Bayer sensor data
 * 
 * This preprocessor improves compression by:
 * 1. Separating color channels (R, G1, G2, B)
 * 2. Applying delta encoding within each channel
 * 3. Deinterleaving bits for better entropy coding
 * 
 * The transformation is reversible and lossless.
 */
class BayerPreprocessor : public IPreprocessor {
public:
    BayerPreprocessor() = default;
    ~BayerPreprocessor() override = default;

    /**
     * @brief Process RAW data to improve compressibility
     * @param data Raw Bayer data
     * @param metadata RAW metadata including pattern detection
     * @return Transformed data ready for compression
     */
    std::vector<uint8_t> process(
        const std::vector<uint8_t>& data,
        const RawMetadata& metadata
    );

    /**
     * @brief Reverse the preprocessing after decompression
     * @param data Compressed and then decompressed data
     * @param metadata RAW metadata (must be same as preprocessing)
     * @return Original Bayer data
     */
    std::vector<uint8_t> reverse(
        const std::vector<uint8_t>& data,
        const RawMetadata& metadata
    );

    /**
     * @brief Detect Bayer pattern from metadata
     * @param data RAW file data (TIFF/DNG format)
     * @return Detected pattern, or Unknown if not found
     */
    static BayerPattern detectPattern(const std::vector<uint8_t>& data);

    /**
     * @brief Extract RAW metadata from TIFF/DNG
     * @param data RAW file data
     * @return Metadata structure, or nullopt if extraction fails
     */
    static std::optional<RawMetadata> extractMetadata(const std::vector<uint8_t>& data);

    std::string name() const override { return "BayerPreprocessor"; }

private:
    /**
     * @brief Separate interleaved Bayer data into color channels
     * @param data Raw interleaved Bayer data
     * @param width Image width
     * @param height Image height
     * @param pattern Bayer pattern type
     * @return Array of 4 channels: [R, G1, G2, B]
     */
    std::array<std::vector<uint16_t>, 4> separateChannels(
        const std::vector<uint8_t>& data,
        uint32_t width,
        uint32_t height,
        BayerPattern pattern
    );

    /**
     * @brief Apply delta encoding to a channel
     * @param channel Input pixel values
     * @param width Channel width (original_width / 2)
     * @param height Channel height (original_height / 2)
     * @return Delta-encoded values (smaller range, better entropy)
     */
    std::vector<int16_t> deltaEncode(
        const std::vector<uint16_t>& channel,
        uint32_t width,
        uint32_t height
    );

    /**
     * @brief Reverse delta encoding
     */
    std::vector<uint16_t> deltaDecode(
        const std::vector<int16_t>& deltaChannel,
        uint32_t width,
        uint32_t height
    );

    /**
     * @brief Interleave separated channels back into Bayer format
     */
    std::vector<uint8_t> interleaveChannels(
        const std::array<std::vector<uint16_t>, 4>& channels,
        uint32_t width,
        uint32_t height,
        BayerPattern pattern
    );
};

} // namespace mosqueeze
```

### Phase 3: Implementation

**File:** `src/libmosqueeze/src/preprocessors/BayerPreprocessor.cpp`

```cpp
#include "mosqueeze/preprocessors/BayerPreprocessor.hpp"
#include <algorithm>
#include <cmath>

namespace mosqueeze {

BayerPattern BayerPreprocessor::detectPattern(const std::vector<uint8_t>& data) {
    // Parse TIFF CFARepeatPatternDim tag (0x828D)
    // Standard patterns:
    // - [2, 2, R, G, G, B] → RGGB
    // - [2, 2, G, R, B, G] → GRBG
    // - [2, 2, G, B, R, G] → GBRG
    // - [2, 2, B, G, G, R] → BGGR
    
    // Implementation requires TIFF parsing
    // See TiffParser from Issue #35
    return BayerPattern::Unknown;
}

std::optional<RawMetadata> BayerPreprocessor::extractMetadata(
    const std::vector<uint8_t>& data
) {
    // Use TiffParser to extract:
    // - Width (tag 256)
    // - Height (tag 257)
    // - BitsPerSample (tag 258)
    // - CFARepeatPatternDim (tag 0x828D)
    // - CFAPattern (tag 0x828E)
    
    return std::nullopt; // Placeholder
}

std::vector<uint8_t> BayerPreprocessor::process(
    const std::vector<uint8_t>& data,
    const RawMetadata& metadata
) {
    // Step 1: Separate channels
    auto channels = separateChannels(
        data,
        metadata.width,
        metadata.height,
        metadata.pattern
    );

    // Step 2: Delta encode each channel
    std::array<std::vector<int16_t>, 4> deltaChannels;
    for (int i = 0; i < 4; ++i) {
        deltaChannels[i] = deltaEncode(
            channels[i],
            metadata.width / 2,
            metadata.height / 2
        );
    }

    // Step 3: Pack into output format
    std::vector<uint8_t> result;
    // ... implementation
    
    return result;
}

std::vector<uint8_t> BayerPreprocessor::reverse(
    const std::vector<uint8_t>& data,
    const RawMetadata& metadata
) {
    // Reverse: unpack → delta decode → interleave
    // ... implementation
    return {};
}

std::array<std::vector<uint16_t>, 4> BayerPreprocessor::separateChannels(
    const std::vector<uint8_t>& data,
    uint32_t width,
    uint32_t height,
    BayerPattern pattern
) {
    std::array<std::vector<uint16_t>, 4> channels;
    
    // Each channel is width/2 × height/2
    size_t channelSize = (width / 2) * (height / 2);
    for (int i = 0; i < 4; ++i) {
        channels[i].reserve(channelSize);
    }

    // Extract pixels based on pattern
    // RGGB: R at (0,0), G at (0,1), G at (1,0), B at (1,1)
    for (uint32_t y = 0; y < height; y += 2) {
        for (uint32_t x = 0; x < width; x += 2) {
            size_t idx = (y * width + x) * 2; // 16-bit pixels
            uint16_t p0 = data[idx] | (data[idx + 1] << 8);
            uint16_t p1 = data[(y * width + x + 1) * 2];
            uint16_t p2 = data[((y + 1) * width + x) * 2];
            uint16_t p3 = data[((y + 1) * width + x + 1) * 2];
            
            // Assign to channels based on pattern
            switch (pattern) {
                case BayerPattern::RGGB:
                    channels[0].push_back(p0); // R
                    channels[1].push_back(p1); // G1
                    channels[2].push_back(p2); // G2
                    channels[3].push_back(p3); // B
                    break;
                // ... other patterns
            }
        }
    }

    return channels;
}

std::vector<int16_t> BayerPreprocessor::deltaEncode(
    const std::vector<uint16_t>& channel,
    uint32_t width,
    uint32_t height
) {
    std::vector<int16_t> delta;
    delta.reserve(channel.size());

    // Simple delta: current - previous (row-major order)
    // More sophisticated: gradient prediction using left + above - diagonal
    
    uint16_t prev = channel[0];
    delta.push_back(static_cast<int16_t>(prev)); // First pixel as-is

    for (size_t i = 1; i < channel.size(); ++i) {
        int32_t diff = static_cast<int32_t>(channel[i]) - static_cast<int32_t>(prev);
        delta.push_back(static_cast<int16_t>(diff));
        prev = channel[i];
    }

    return delta;
}

} // namespace mosqueeze
```

### Phase 4: Integration

**File:** `src/libmosqueeze/src/PreprocessorSelector.cpp`

Add Bayer preprocessor registration:

```cpp
#include "mosqueeze/preprocessors/BayerPreprocessor.hpp"

// In PreprocessorSelector::select()
if (fileType == FileType::NikonRaw ||
    fileType == FileType::CanonRaw ||
    fileType == FileType::FujifilmRaw ||
    fileType == FileType::DNG ||
    fileType == FileType::SonyRaw) {
    
    preprocessor = std::make_unique<BayerPreprocessor>();
}
```

---

## Files to Create/Modify

| File | Action |
|------|--------|
| `src/libmosqueeze/include/mosqueeze/preprocessors/BayerPreprocessor.hpp` | Create |
| `src/libmosqueeze/src/preprocessors/BayerPreprocessor.cpp` | Create |
| `src/libmosqueeze/src/PreprocessorSelector.cpp` | Modify (add registration) |
| `tests/unit/BayerPreprocessor_test.cpp` | Create |

---

## Testing Plan

### Unit Tests

```cpp
TEST(BayerPreprocessor, DetectsPatternRGGB) {
    std::vector<uint8_t> dng = loadDngWithPattern(BayerPattern::RGGB);
    EXPECT_EQ(BayerPreprocessor::detectPattern(dng), BayerPattern::RGGB);
}

TEST(BayerPreprocessor, SeparatesChannelsCorrect) {
    // Create 4x4 Bayer image: RGGB pattern
    std::vector<uint16_t> bayer = {
        100, 200, 110, 210,  // Row 0: R G R G
        150, 250, 160, 260,  // Row 1: G B G B
        105, 205, 115, 215,  // Row 2: R G R G
        155, 255, 165, 265   // Row 3: G B G B
    };
    
    BayerPreprocessor preproc;
    auto channels = preproc.separateChannels(bayer, 4, 4, BayerPattern::RGGB);
    
    EXPECT_EQ(channels[0].size(), 4); // R channel: 100, 110, 105, 115
    EXPECT_EQ(channels[3].size(), 4); // B channel: 250, 260, 255, 265
}

TEST(BayerPreprocessor, DeltaEncodeReducesEntropy) {
    std::vector<uint16_t> constant(100, 1000);
    auto delta = BayerPreprocessor::deltaEncode(constant, 10, 10);
    
    // All deltas should be 0 (or near 0)
    EXPECT_NEAR(delta[1], 0, 1);
}

TEST(BayerPreprocessor, RoundtripPreservesData) {
    std::vector<uint8_t> original = loadTestRaw("test.nef");
    RawMetadata meta = *BayerPreprocessor::extractMetadata(original);
    
    BayerPreprocessor preproc;
    auto processed = preproc.process(original, meta);
    auto restored = preproc.reverse(processed, meta);
    
    EXPECT_EQ(original, restored);
}
```

### Integration Test with Compression

```cpp
TEST(BayerPreprocessor, ImprovesCompressionRatio) {
    std::vector<uint8_t> raw = loadTestRaw("test.nef");
    
    // Compress without preprocessing
    ZstdEngine zstd;
    auto result1 = zstd.compress(raw, CompressionOptions{.level = 22});
    double ratio1 = static_cast<double>(raw.size()) / result1.compressedBytes;
    
    // Compress with preprocessing
    BayerPreprocessor preproc;
    RawMetadata meta = *BayerPreprocessor::extractMetadata(raw);
    auto preprocessed = preproc.process(raw, meta);
    
    auto result2 = zstd.compress(preprocessed, CompressionOptions{.level = 22});
    double ratio2 = static_cast<double>(raw.size()) / result2.compressedBytes;
    
    EXPECT_GT(ratio2, ratio1); // Preprocessed should compress better
}
```

---

## Dependencies

| Dependency | Status |
|------------|--------|
| Issue #35 (RAW format detection) | Required — needs TiffParser |
| PR #32 (Preprocessing Engine) | Merged — provides framework |

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Pattern detection fails | Fall back to no preprocessing |
| Performance overhead | Benchmark acceptable limits, optimize later |
| Non-standard patterns | Support common patterns first, extend later |
| Memory usage for large RAWs | Stream processing in chunks |

---

## Implementation Phases

| Phase | Task | Duration |
|-------|------|----------|
| 1 | Research existing implementations | 2 days |
| 2 | BayerPreprocessor interface + tests | 1 day |
| 3 | Pattern detection with TiffParser | 2 days |
| 4 | Delta encoding + channel separation | 2 days |
| 5 | Integration + benchmarking | 2 days |
| **Total** | | **9 days** |

---

## Verification Checklist

- [ ] Pattern detection works for RGGB, GRBG, GBRG, BGGR
- [ ] Metadata extraction from NEF, RAF, CR2, DNG
- [ ] Channel separation produces correct dimensions
- [ ] Delta encoding reduces entropy
- [ ] Roundtrip is lossless
- [ ] Compression ratio improves by measurable amount
- [ ] Performance is acceptable (< 100ms for typical RAW)
- [ ] Integration with PreprocessorSelector works

---

## References

- [Bayer filter - Wikipedia](https://en.wikipedia.org/wiki/Bayer_filter)
- [JPEG XL modular mode](https://google.github.io/au/jpegxl/#modular-mode) — context modeling
- [FLIF compression](https://flif.info/spec.html) — MANIAC trees
- [DNG 1.6 Specification](https://www.adobe.io/content/dam/dw/1658705319505/dng/dng_spec_1.6.pdf) — CFA tags
- [libraw source](https://github.com/LibRaw/LibRaw) — reference implementation