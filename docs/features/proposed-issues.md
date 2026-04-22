# Proposed GitHub Issues

Token API limitato - creare manualmente da GitHub UI.

---

## Issue: oxipng Integration for PNG Optimization

**Category:** enhancement, preprocessor, performance

### Summary
Integrate [oxipng](https://github.com/shssoichiro/oxipng) as an alternative PNG preprocessor for better compression ratios than libpng.

### Motivation
libpng max compression (level 9) achieves ~10-30% improvement, but oxipng can achieve additional 5-15% through:
- Better filter selection heuristics
- PNGOUT-compatible recompression
- Zopfli-style DEFLATE compression

### Proposed Implementation
1. Add oxipng as optional dependency (or compile from source)
2. Create `OxipngOptimizer` class under `IPreprocessor`
3. Add `--png-engine {libpng,oxipng}` CLI flag
4. Benchmark comparison on same corpus

### Algorithm Choice
- **libpng (fast)**: Level 6, single filter - good for realtime
- **libpng (balanced)**: Level 9, filter testing - moderate speed
- **oxipng (maximum)**: All filters + Zopfli - slower but better compression

### Related
- #13: PNG Re-compression Optimization (base implementation)
- Uses libpng as initial implementation

### Acceptance Criteria
- [ ] oxipng integration working
- [ ] CLI flag for engine selection
- [ ] Benchmark shows oxipng improvement over libpng
- [ ] Fallback to libpng if oxipng not available

---

## Issue: Preprocessor Unit Test Suite

**Category:** enhancement, testing, quality

### Summary
Create comprehensive unit tests for all preprocessors to prevent regressions and validate correctness.

### Motivation
Preprocessors transform file data before compression. Bugs can:
- Corrupt data (lossy when should be lossless)
- Miss optimization opportunities (ineffective transform)
- Crash on edge cases (malformed input)

Recent BayerPreprocessor bug (#46) processed entire RAF file instead of just image data - unit tests would have caught this.

### Test Categories

#### 1. Correctness Tests
- Round-trip: `decompress(preprocess(compress(data)))` equals original
- Pixel-perfect for image preprocessors
- Semantic equivalence for JSON/XML canonicalizers

#### 2. Edge Cases
- Empty files
- Already optimally compressed
- Malformed input handling
- Large files (>1GB)
- Binary metadata embedded in images

#### 3. Performance Tests
- Benchmark suite for each preprocessor
- Memory usage limits
- Multi-threaded correctness

### Proposed Test Suite
```
tests/preprocessors/
├── BayerPreprocessor_test.cpp
├── PngOptimizer_test.cpp
├── ImageMetaStripper_test.cpp
├── JsonCanonicalizer_test.cpp
├── XmlCanonicalizer_test.cpp
└── fixtures/
    ├── sample.raf
    ├── sample.png
    └── sample.json
```

### Related
- #46: BayerPreprocessor RAF parsing bug
- #13: PNG optimization

### Acceptance Criteria
- [ ] Unit tests for all 5 preprocessors
- [ ] CI integration with coverage reporting
- [ ] Test fixtures for common file formats

---

## Issue: Support for Uncompressed RAW Formats

**Category:** enhancement, preprocessor, compatibility

### Summary
Add BayerPreprocessor support for truly uncompressed RAW formats (not Fujifilm Lossless Compressed).

### Problem
Current BayerPreprocessor tests show **no improvement** on Fujifilm RAF files because:
- Fujifilm "Lossless Compressed RAW" is already ~2x compressed internally
- 27MB compressed vs ~52MB uncompressed equivalent
- Bayer patterns are destroyed by internal compression entropy

### Supported Formats (should benefit)
- **Phase One IIQ** (uncompressed option)
- **Hasselblad 3FR** (uncompressed option)
- **Sony ARW** (uncompressed mode)
- **Nikon NEF** (uncompressed mode)
- **Canon CR3/CR2** (uncompressed mode)
- **DNG** (uncompressed option)

### Proposed Implementation
1. Add format detection based on magic bytes
2. Skip BayerPreprocessor for already-compressed RAW
3. Add `--force-bayer` flag to process anyway
4. Document format compatibility in README

### Format Detection
```cpp
enum class RawCompression {
    Uncompressed,    // BayerPreprocessor beneficial
    LosslessCompressed, // BayerPreprocessor ineffective
    LossyCompressed,    // BayerPreprocessor HARMFUL - reject
    Unknown             // Conservative: skip
};
```

### Benchmark Targets
Expected improvement on uncompressed RAW:
- Phase One IIQ: 15-25% additional compression
- Sony ARW uncompressed: 10-20%
- Hasselblad 3FR: 15-25%

### Related
- #46: BayerPreprocessor RAF parsing
- BayerPreprocessor processes entire RAF instead of just Bayer data

### Acceptance Criteria
- [ ] Format detection for major RAW types
- [ ] Auto-skip for lossless-compressed RAW
- [ ] Documentation of supported formats
- [ ] Benchmark results on uncompressed RAW