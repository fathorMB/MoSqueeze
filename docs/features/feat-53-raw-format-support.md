# Worker Spec: Support for Uncompressed RAW Formats

**Issue:** #53 - Feature: Support for Uncompressed RAW Formats  
**Branch:** `feat/raw-format-support`  
**Priority:** Medium  
**Estimated Effort:** 2-3 days  
**Depends on:** #46 (BayerPreprocessor RAF parsing - MERGED)  

---

## Overview

Extend BayerPreprocessor to truly uncompressed RAW formats. The current implementation shows **no improvement** on Fujifilm RAF files because they use "Lossless Compressed RAW" internally. This feature adds format detection and proper handling for uncompressed RAW formats.

---

## Problem Statement

### Current findings (BayerPreprocessor benchmark v2)

| File | Original | Compressed | Ratio | With BayerPreprocessor | Ratio |
|------|----------|------------|-------|------------------------|-------|
| RAF (compressed) | 27MB | 24.3MB | 0.90x | 24.3MB | 0.90x **(no improvement)** |

**Root cause:** Fujifilm "Lossless Compressed RAW":
- Camera compresses RAW data internally (~2x compression)
- 27MB compressed vs ~52MB uncompressed equivalent
- Bayer patterns destroyed by compression entropy
- BayerPreprocessor cannot improve already-compressed data

### What we need

**Truly uncompressed RAW** where BayerPreprocessor provides benefit:
- Uncompressed Phase One IIQ
- Uncompressed Hasselblad 3FR  
- Uncompressed Sony ARW
- Uncompressed Nikon NEF
- Uncompressed Canon CR3/CR2
- Uncompressed DNG

**Expected improvement:** 15-25% additional compression on uncompressed RAW.

---

## Solution Architecture

### Format Detection System

```cpp
// src/libmosqueeze/include/mosqueeze/RawFormat.hpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mosqueeze {

enum class RawCompression {
    Uncompressed,        // BayerPreprocessor beneficial ✓
    LosslessCompressed,  // BayerPreprocessor ineffective ✗
    LossyCompressed,     // BayerPreprocessor HARMFUL - reject ⚠
    Unknown              // Conservative: skip ?
};

struct RawFormatInfo {
    std::string_view extension;      // ".RAF", ".ARW", etc.
    std::string_view manufacturer;    // "Fujifilm", "Sony", etc.
    RawCompression compression;      // Detected compression type
    std::string_view compressionName; // "Lossless Compressed RAW"
    bool isUncompressedAvailable;    // Camera supports uncompressed mode
    std::string_view notes;          // Human-readable notes
};

class RawFormatDetector {
public:
    // Detect format from magic bytes
    static std::optional<RawFormatInfo> detect(std::span<const uint8_t> data);
    
    // Detect from file extension (fallback)
    static std::optional<RawFormatInfo> detectByExtension(std::string_view filename);
    
    // Check if BayerPreprocessor should be applied
    static bool shouldApplyBayerPreprocessor(RawCompression compression) {
        return compression == RawCompression::Uncompressed;
    }
    
private:
    // Magic byte signatures
    static constexpr auto FUJIFILM_RAF = std::array{'F', 'U', 'J', 'I', 'F', 'I', 'L', 'M'};
    static constexpr auto SONY_ARW = std::array{'S', 'O', 'N', 'Y', ' ', 'D', 'S', 'C'};
    static constexpr auto CANON_CR2 = std::array{'I', 'I', '*', 0x10, 0x00, 0x00, 0x00, 0x43};
    static constexpr auto CANON_CR3 = std::array{0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70};
    static constexpr auto NIKON_NEF = std::array{'N', 'E', 'F', 0x00};
    static constexpr auto PHASEONE_IIQ = std::array{'I', 'I', 'Q', 0x00};
    static constexpr auto HASSELBLAD_3FR = std::array{'H', '3', 'D'}; // or '3','F','R'
    static constexpr auto ADOBE_DNG = std::array{'I', 'I', '*', 0x00, 0x00, 0x00, 0x14}; // TIFF-based
};

} // namespace mosqueeze
```

### Format Detection Implementation

```cpp
// src/libmosqueeze/src/RawFormat.cpp
#include "mosqueeze/RawFormat.hpp"
#include <algorithm>

namespace mosqueeze {

std::optional<RawFormatInfo> RawFormatDetector::detect(std::span<const uint8_t> data) {
    if (data.size() < 16) return std::nullopt;
    
    // Fujifilm RAF
    if (std::equal(FUJIFILM_RAF.begin(), FUJIFILM_RAF.end(), data.begin())) {
        // Check compression type in RAF header
        // Offset 84: compression type (1 = lossless compressed, 0 = uncompressed)
        uint8_t compressionType = data.size() > 84 ? data[84] : 0xFF;
        return RawFormatInfo{
            .extension = ".RAF",
            .manufacturer = "Fujifilm",
            .compression = (compressionType == 1) ? RawCompression::LosslessCompressed 
                                                    : RawCompression::Uncompressed,
            .compressionName = (compressionType == 1) ? "Lossless Compressed RAW" 
                                                        : "Uncompressed RAW",
            .isUncompressedAvailable = true,
            .notes = "Fujifilm default is Lossless Compressed; Uncompressed mode available"
        };
    }
    
    // Sony ARW
    if (std::equal(SONY_ARW.begin(), SONY_ARW.end(), data.begin())) {
        // Sony ARW 1.0/2.0/2.1/2.2/2.3/3.0
        // Check bit depth and compression flag
        uint16_t version = (data[8] << 8) | data[9];
        // Most Sony RAWs are compressed by default
        return RawFormatInfo{
            .extension = ".ARW",
            .manufacturer = "Sony",
            .compression = RawCompression::Unknown, // Need more context
            .compressionName = "Unknown",
            .isUncompressedAvailable = true,
            .notes = "Check camera settings for 'Uncompressed RAW' option"
        };
    }
    
    // Canon CR2 (TIFF-based)
    if (std::equal(CANON_CR2.begin(), CANON_CR2.end(), data.begin())) {
        // CR2 version is at offset 8
        uint16_t cr2Version = (data[8] << 8) | data[9];
        return RawFormatInfo{
            .extension = ".CR2",
            .manufacturer = "Canon",
            .compression = RawCompression::Unknown,
            .compressionName = "Unknown",
            .isUncompressedAvailable = true,
            .notes = "Check compression tag in EXIF"
        };
    }
    
    // Canon CR3 (ISOBMFF-based)
    if (std::equal(CANON_CR3.begin(), CANON_CR3.end(), data.begin())) {
        return RawFormatInfo{
            .extension = ".CR3",
            .manufacturer = "Canon",
            .compression = RawCompression::Unknown,
            .compressionName = "Unknown",
            .isUncompressedAvailable = true,
            .notes = "CR3 uses image tiles; check tile compression"
        };
    }
    
    // Nikon NEF
    if (std::equal(NIKON_NEF.begin(), NIKON_NEF.end(), data.begin())) {
        return RawFormatInfo{
            .extension = ".NEF",
            .manufacturer = "Nikon",
            .compression = RawCompression::Unknown,
            .compressionName = "Unknown",
            .isUncompressedAvailable = true,
            .notes = "Nikon default is Lossless Compressed"
        };
    }
    
    // Phase One IIQ
    if (std::equal(PHASEONE_IIQ.begin(), PHASEONE_IIQ.end(), data.begin())) {
        return RawFormatInfo{
            .extension = ".IIQ",
            .manufacturer = "Phase One",
            .compression = RawCompression::Uncompressed, // Usually uncompressed
            .compressionName = "Uncompressed",
            .isUncompressedAvailable = true,
            .notes = "Phase One IIQ is typically uncompressed"
        };
    }
    
    // Hasselblad 3FR
    if (std::equal(HASSELBLAD_3FR.begin(), HASSELBLAD_3FR.end(), data.begin())) {
        return RawFormatInfo{
            .extension = ".3FR",
            .manufacturer = "Hasselblad",
            .compression = RawCompression::Uncompressed,
            .compressionName = "Uncompressed",
            .isUncompressedAvailable = true,
            .notes = "Hasselblad 3FR is typically uncompressed"
        };
    }
    
    // Adobe DNG
    if (std::equal(ADOBE_DNG.begin(), ADOBE_DNG.end(), data.begin())) {
        // DNG can be anything - check SubIFD compression tag
        return RawFormatInfo{
            .extension = ".DNG",
            .manufacturer = "Adobe",
            .compression = RawCompression::Unknown,
            .compressionName = "Depends on source",
            .isUncompressedAvailable = true,
            .notes = "DNG compression follows source camera"
        };
    }
    
    return std::nullopt; // Unknown format
}

std::optional<RawFormatInfo> RawFormatDetector::detectByExtension(std::string_view filename) {
    auto dot = filename.rfind('.');
    if (dot == std::string_view::npos) return std::nullopt;
    
    auto ext = filename.substr(dot);
    // Convert to uppercase
    std::string extUpper(ext);
    std::transform(extUpper.begin(), extUpper.end(), extUpper.begin(), ::toupper);
    
    if (extUpper == ".RAF") {
        return RawFormatInfo{
            .extension = ".RAF",
            .manufacturer = "Fujifilm",
            .compression = RawCompression::LosslessCompressed, // Default assumption
            .compressionName = "Lossless Compressed RAW (assumed)",
            .isUncompressedAvailable = true,
            .notes = "Most Fujifilm RAF is Lossless Compressed"
        };
    }
    
    // Similar for other formats...
    return std::nullopt;
}

} // namespace mosqueeze
```

---

## BayerPreprocessor Integration

### Updated BayerPreprocessor.cpp

```cpp
// src/libmosqueeze/src/preprocessors/BayerPreprocessor.cpp

#include "mosqueeze/RawFormat.hpp"
#include "mosqueeze/preprocessors/BayerPreprocessor.hpp"

namespace mosqueeze {

std::expected<std::vector<uint8_t>, PreprocessError>
BayerPreprocessor::preprocess(std::span<const uint8_t> data) {
    // Detect RAW format
    auto format = RawFormatDetector::detect(data);
    
    if (!format) {
        // Unknown format - conservative: try to process
        // (may work on uncompressed RAW from unknown cameras)
        spdlog::warn("Unknown RAW format, attempting BayerPreprocessor anyway");
    } else {
        // Log detected format
        spdlog::info("Detected {} {} ({})", 
            format->manufacturer, 
            format->extension, 
            format->compressionName);
        
        // Check if BayerPreprocessor is appropriate
        if (!RawFormatDetector::shouldApplyBayerPreprocessor(format->compression)) {
            if (format->compression == RawCompression::LosslessCompressed) {
                spdlog::warn(
                    "{} uses {} - BayerPreprocessor provides no benefit. "
                    "Use --force-bayer to process anyway.",
                    format->manufacturer, 
                    format->compressionName
                );
                
                if (!forceProcess_) {
                    return std::unexpected(PreprocessError{
                        .code = ErrorCode::FormatNotSupported,
                        .message = fmt::format(
                            "{} is already compressed ({}). BayerPreprocessor ineffective.",
                            format->manufacturer,
                            format->compressionName
                        )
                    });
                }
            } else if (format->compression == RawCompression::LossyCompressed) {
                spdlog::error(
                    "{} uses lossy compression - BayerPreprocessor would HARM data!",
                    format->manufacturer
                );
                return std::unexpected(PreprocessError{
                    .code = ErrorCode::HarmfulOperation,
                    .message = fmt::format(
                        "{} is lossy compressed. BayerPreprocessor would corrupt data.",
                        format->manufacturer
                    )
                });
            }
        }
    }
    
    // Continue with existing BayerPreprocessor logic...
    // (parse RAF header, extract Bayer data, transform, etc.)
}

} // namespace mosqueeze
```

---

## CLI Integration

### New Flags

```cpp
// --preprocess bayer-raw behavior changes:
// 1. Detects format and compression type
// 2. Skips already-compressed RAW (with warning)
// 3. Rejects lossy-compressed RAW (error)
// 4. Processes uncompressed RAW normally

// --force-bayer
// Processes even if format is detected as already-compressed
// Use ONLY if you know the file is truly uncompressed

// --dry-run
// Reports what would be done without processing
// Useful for "what compression type is this RAW?"
```

### CLI Output

```
$ mosqueeze-bench -d ./raw/ -a zstd --preprocess bayer-raw

Detecting RAW formats...

[DNG] ./phase1.iiq
  → Phase One IIQ (Uncompressed) - BayerPreprocessor will be applied ✓

[RAF] ./fuji.raf
  → Fujifilm RAF (Lossless Compressed RAW) - SKIPPED
  ⚠ BayerPreprocessor provides no benefit. Use --force-bayer to process anyway.

[ARW] ./sony.arw
  → Sony ARW (compression unknown) - Proceeding with caution
  ℹ If this is Lossless Compressed, BayerPreprocessor won't help.

Processing ./phase1.iiq with BayerPrepressor...
  Original: 52.3 MB
  Post-BayerPreprocessor: 48.1 MB (7-8% reduction)
  Final (zstd): 42.2 MB (19% total compression)
```

---

## Test Cases

```cpp
TEST_CASE("RawFormatDetector: Fujifilm RAF Lossless Compressed") {
    auto data = loadRafFile("fuji_lossless.raf");
    auto format = RawFormatDetector::detect(data);
    REQUIRE(format.has_value());
    REQUIRE(format->extension == ".RAF");
    REQUIRE(format->compression == RawCompression::LosslessCompressed);
    REQUIRE_FALSE(RawFormatDetector::shouldApplyBayerPreprocessor(format->compression));
}

TEST_CASE("RawFormatDetector: Fujifilm RAF Uncompressed") {
    auto data = loadRafFile("fuji_uncompressed.raf");
    auto format = RawFormatDetector::detect(data);
    REQUIRE(format.has_value());
    REQUIRE(format->compression == RawCompression::Uncompressed);
    REQUIRE(RawFormatDetector::shouldApplyBayerPreprocessor(format->compression));
}

TEST_CASE("RawFormatDetector: Phase One IIQ") {
    auto data = loadIiqFile("phase1.iiq");
    auto format = RawFormatDetector::detect(data);
    REQUIRE(format.has_value());
    REQUIRE(format->extension == ".IIQ");
    REQUIRE(format->compression == RawCompression::Uncompressed);
}

TEST_CASE("BayerPreprocessor: Skip Lossless Compressed") {
    BayerPreprocessor bp;
    auto data = loadRafFile("fuji_lossless.raf");
    auto result = bp.preprocess(data);
    REQUIRE(!result.has_value());
    REQUIRE(result.error().code == ErrorCode::FormatNotSupported);
}

TEST_CASE("BayerPreprocessor: Force Process Compressed") {
    BayerPreprocessor bp(/*force=*/true);
    auto data = loadRafFile("fuji_lossless.raf");
    auto result = bp.preprocess(data);
    REQUIRE(result.has_value()); // Processes anyway
}

TEST_CASE("BayerPreprocessor: Reject Lossy Compressed") {
    BayerPreprocessor bp(/*force=*/true);
    auto data = loadFile("lossy_compressed.raw");
    auto result = bp.preprocess(data);
    REQUIRE(!result.has_value());
    REQUIRE(result.error().code == ErrorCode::HarmfulOperation);
}
```

---

## Implementation Steps

### Phase 1: Format Detection (1 day)
- [ ] Create `RawFormat.hpp` with enum and struct
- [ ] Implement `RawFormatDetector::detect()` for major formats
- [ ] Add magic byte signatures for all supported formats
- [ ] Write unit tests for format detection
- [ ] Add format detection to CLI `--dry-run` mode

### Phase 2: BayerPreprocessor Integration (1 day)
- [ ] Add `force_` flag to BayerPreprocessor
- [ ] Add format check at start of `preprocess()`
- [ ] Skip with warning for LosslessCompressed
- [ ] Reject with error for LossyCompressed
- [ ] Log detected format and compression type

### Phase 3: CLI Updates (0.5 day)
- [ ] Add `--force-bayer` flag
- [ ] Add `--dry-run` detection mode
- [ ] Update help text with format support info
- [ ] Add format support documentation to README

### Phase 4: Testing (0.5 day)
- [ ] Create test fixtures for each format type
- [ ] Test with actual uncompressed RAW files (if available)
- [ ] Verify rejection of lossy formats
- [ ] Benchmark on uncompressed RAW (expect 15-25% improvement)

---

## Test Fixtures Needed

1. **Fujifilm RAF** - Both compressed and uncompressed (if possible)
2. **Phase One IIQ** - Typically uncompressed
3. **Hasselblad 3FR** - Typically uncompressed  
4. **Sony ARW** - Compressed and uncompressed variants
5. **Canon CR2/CR3** - Compressed and uncompressed variants
6. **Nikon NEF** - Compressed and uncompressed variants
7. **Adobe DNG** - Various source formats

> Note: May need to source test files from photographers or generate synthetic RAW data.

---

## Benchmark Targets

Expected BayerPreprocessor improvement on **uncompressed** RAW:

| Format | Typical Size | Expected Improvement |
|--------|-------------|---------------------|
| Phase One IIQ | 50-80 MB | 15-25% |
| Hasselblad 3FR | 40-70 MB | 15-25% |
| Sony ARW (uncompressed) | 40-50 MB | 10-20% |
| Nikon NEF (uncompressed) | 35-45 MB | 10-15% |
| DNG (uncompressed) | Varies | 15-25% |

---

## Documentation

### README.md Addition

```markdown
## Supported RAW Formats

BayerPreprocessor effectiveness depends on RAW compression type:

| Manufacturer | Format | Compression | BayerPreprocessor |
|--------------|--------|-------------|-------------------|
| Fujifilm | RAF | Lossless Compressed | ⚠ No benefit (default) |
| Fujifilm | RAF | Uncompressed | ✓ Beneficial |
| Phase One | IIQ | Uncompressed | ✓ Beneficial |
| Hasselblad | 3FR | Uncompressed | ✓ Beneficial |
| Sony | ARW | Uncompressed | ✓ Beneficial |
| Sony | ARW | Compressed | ⚠ No benefit |
| Nikon | NEF | Uncompressed | ✓ Beneficial |
| Nikon | NEF | Lossless Compressed | ⚠ No benefit |
| Canon | CR2/CR3 | Uncompressed | ✓ Beneficial |
| Canon | CR2/CR3 | Compressed | ⚠ No benefit |
| Adobe | DNG | Depends on source | Check original |

Use `mosqueeze-bench --preprocess bayer-raw --dry-run` to check your files.
```

---

## Acceptance Criteria

- [ ] Format detection for Fujifilm RAF, Sony ARW, Canon CR2/CR3, Nikon NEF, Phase One IIQ, Hasselblad 3FR, Adobe DNG
- [ ] Auto-skip for LosslessCompressed RAW with warning
- [ ] Reject LossyCompressed RAW with error (cannot be processed safely)
- [ ] `--force-bayer` flag to override skip behavior
- [ ] `--dry-run` mode for format detection only
- [ ] Unit tests for all supported formats
- [ ] Documentation updated with format support table
- [ ] CLI reports detected format and compression type
- [ ] Benchmark on at least one uncompressed RAW format (if available)

---

## Related Issues

- #46: BayerPreprocessor RAF parsing (foundation)
- #52: Preprocessor Unit Test Suite (tests needed)

---

## Risks

1. **Test files availability** - Uncompressed RAW files are large and may be hard to source
2. **Format variations** - Camera firmware may change format details
3. **False positives** - Incorrect format detection could cause data loss
4. **Performance** - Format detection adds minimal overhead but should be profiled

---

## Future Enhancements

- Automatic compression detection via EXIF/TIFF tags
- Support for additional RAW formats (Panasonic RW2, Olympus ORF, etc.)
- Synthetic test fixture generation
- Camera model specific optimizations