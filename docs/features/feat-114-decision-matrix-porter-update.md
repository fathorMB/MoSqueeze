# Worker Spec: DecisionMatrix Porter Benchmark Update

**Issue**: [#114](https://github.com/fathorMB/MoSqueeze/issues/114)  
**Type**: Enhancement  
**Priority**: P2-Medium  
**Severity**: Minor (Performance)  
**Estimated Effort**: 2-3 hours

---

## Summary

Porter's benchmark on already-compressed file types (FLAC, PDF) revealed that secondary compression provides minimal gain (3-10%). The DecisionMatrix and AlgorithmSelector should skip compression for these types to save CPU cycles and improve throughput.

---

## Problem Statement

### Current Behavior

1. **FLAC files** are detected but may attempt compression
   - Porter benchmark: 54,060 tests, avg ratio 1.09x
   - CPU wasted for < 10% gain
   - Entropy threshold: ~7.57 bit/byte (high)

2. **PDF files** detected as `Document_PDF` (after #109) but compression attempted
   - Porter benchmark: 100 tests, avg ratio 1.03x
   - Negligible gain (3%) 
   - Entropy threshold: ~7.97 bit/byte (very high)

3. **No entropy-based skip threshold**
   - High-entropy files (>7.5) should likely be skipped
   - Currently no logic to check entropy before compression

### Evidence from Porter Benchmark

```
FLAC LibriSpeech (2,703 files, 54,060 tests):
  - brotli/11: 1.097x (22ms)
  - zstd/6:    1.097x (0ms)
  - Entropy: 7.57 bit/byte
  - RECOMMENDATION: SKIP compression

PDF Gutenberg (5 files, 100 tests):
  - brotli/11: 1.04x (20ms)
  - zstd/3:    1.03x (0ms)
  - Entropy: 7.97 bit/byte
  - RECOMMENDATION: SKIP compression

Text (BOOKS.TXT, 102 files, 2,040 tests):
  - brotli/11: 2.57x (23ms) — compressible
  - zstd/3:    2.23x (0ms) — fast
  - Entropy: 4.05 bit/byte
  - RECOMMENDATION: Compress (good candidate)
```

---

## Solution

### Design Decision

Add skip rules for already-compressed formats and implement entropy-based early-exit in algorithm selection.

### Implementation

#### 1. Update SelectionRules.json

```json
// config/selection_rules.json

{
  "skip_compression": {
    "extensions": [".7z", ".gz", ".jpg", ".jpeg", ".mov", ".mp4", 
                   ".png", ".wav", ".webp", ".heic", ".heif", ".z",
                   ".flac", ".pdf", ".aac"],
    "file_types": [
      "Audio_FLAC",
      "Document_PDF"
    ],
    "entropy_threshold": 7.5
  },
  // ... existing rules ...
}
```

#### 2. Update AlgorithmSelector

```cpp
// src/libmosqueeze/src/AlgorithmSelector.cpp

std::optional<Recommendation> AlgorithmSelector::select(
    const FileClassification& classification,
    const FileFeatures& features) const {
    
    // Early exit: Check skip list
    if (shouldSkip(classification, features)) {
        return std::nullopt;  // No compression recommended
    }
    
    // ... existing logic ...
}

bool AlgorithmSelector::shouldSkip(
    const FileClassification& classification,
    const FileFeatures& features) const {
    
    // Extension-based skip
    if (skipExtensions_.count(classification.extension)) {
        return true;
    }
    
    // File type-based skip
    if (skipFileTypes_.count(classification.type)) {
        return true;
    }
    
    // Entropy-based skip (high entropy = already compressed)
    if (features.entropy > config_.entropyThreshold) {
        LOG_DEBUG("Skipping compression: high entropy (" 
                  << features.entropy << " > " << config_.entropyThreshold << ")");
        return true;
    }
    
    return false;
}
```

#### 3. Add Config Parameters

```cpp
// src/libmosqueeze/include/mosqueeze/Types.hpp

struct SelectionConfig {
    double entropyThreshold = 7.5;  // Skip if entropy > this value
    std::set<std::string> skipExtensions;
    std::set<FileType> skipFileTypes;
    // ... existing fields ...
};
```

#### 4. Update DecisionMatrix.cpp

```cpp
// src/libmosqueeze/src/DecisionMatrix.cpp

// Add FLAC and PDF entries with explicit "skip" recommendation
{"Audio_FLAC", {
    .algorithm = "none",
    .level = 0,
    .expectedRatio = 1.0,
    .reason = "Already FLAC compressed, secondary compression < 10% gain",
    .skipCompression = true
}},
{"Document_PDF", {
    .algorithm = "none",
    .level = 0,
    .expectedRatio = 1.0,
    .reason = "PDF internal compression often sufficient, < 5% gain",
    .skipCompression = true
}},
// ... other entries ...
```

#### 5. Update FileTypeDetector (if needed)

```cpp
// src/libmosqueeze/src/FileTypeDetector.cpp

// Ensure FLAC is properly detected
{"Audio_FLAC", FileType::Audio_FLAC, "audio/flac", true, true, ".flac"},
//                                     ^^^ isCompressed ^^^ canRecompress

// Note: canRecompress = true for FLAC/PDF means we CAN try recompression,
// but DecisionSelector will recommend skipping based on entropy/ratio.
```

---

## Files to Modify

| File | Changes | Lines |
|------|---------|-------|
| `config/selection_rules.json` | Add FLAC/PDF to skip list, entropy threshold | ~10 lines |
| `src/libmosquette/src/AlgorithmSelector.cpp` | Add shouldSkip() logic | ~20 lines |
| `src/libmosqueeze/include/mosqueeze/Types.hpp` | SelectionConfig fields | ~5 lines |
| `src/libmosqueeze/src/DecisionMatrix.cpp` | Add FLAC/PDF skip entries | ~10 lines |
| `docs/benchmarks/combined_decision_matrix.json` | Already updated ✅ | — |

**Total: ~45 lines of code changes**

---

## Acceptance Criteria

### Must Have

1. **FLAC files skip compression by default**
   ```bash
   mosqueeze predict audio.flac
   # Expected: "No compression recommended" or similar
   ```

2. **PDF files skip compression by default**
   ```bash
   mosqueeze predict document.pdf
   # Expected: "No compression recommended" or similar
   ```

3. **Entropy threshold respected**
   ```bash
   # High entropy file (>7.5) should skip
   mosqueeze analyze high_entropy.bin
   # Expected: Shows entropy > 7.5 + skip recommendation
   ```

4. **Text files still compress correctly**
   ```bash
   mosqueeze predict books.txt
   # Expected: "zstd/3" or "brotli/6" recommendation
   ```

### Should Have

5. **Force compression override**
   ```bash
   # User can force compression even for skip types
   mosqueeze compress audio.flac --force
   # Expected: Compresses despite skip recommendation
   ```

6. **Unit tests pass**
   ```bash
   cd build && ctest -R AlgorithmSelector
   ```

---

## Testing Plan

### Unit Tests

```cpp
// tests/unit/AlgorithmSelector_test.cpp

TEST(AlgorithmSelector, SkipsFLAC) {
    AlgorithmSelector selector(/* config with entropyThreshold=7.5 */);
    
    FileClassification classification{
        .type = FileType::Audio_FLAC,
        .extension = ".flac",
        .isCompressed = true
    };
    
    FileFeatures features{.entropy = 7.57};
    
    auto result = selector.select(classification, features);
    
    EXPECT_FALSE(result.has_value());  // No compression recommended
}

TEST(AlgorithmSelector, SkipsPDF) {
    AlgorithmSelector selector;
    
    FileClassification classification{
        .type = FileType::Document_PDF,
        .extension = ".pdf"
    };
    
    FileFeatures features{.entropy = 7.97};
    
    auto result = selector.select(classification, features);
    
    EXPECT_FALSE(result.has_value());
}

TEST(AlgorithmSelector, CompressesText) {
    AlgorithmSelector selector;
    
    FileClassification classification{
        .type = FileType::Text_Plain,
        .extension = ".txt"
    };
    
    FileFeatures features{.entropy = 4.05};  // Low entropy
    
    auto result = selector.select(classification, features);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->algorithm == "brotli" || result->algorithm == "zstd");
}

TEST(AlgorithmSelector, EntropyThresholdSkip) {
    AlgorithmSelector selector(/* config with entropyThreshold=7.5 */);
    
    FileClassification classification{
        .type = FileType::Binary_Chunked,
        .extension = ".bin"
    };
    
    FileFeatures features{.entropy = 7.8};  // High entropy
    
    auto result = selector.select(classification, features);
    
    EXPECT_FALSE(result.has_value());  // Skip due to entropy
}
```

### Integration Tests

```bash
# Test with real FLAC from Porter corpus
wget -O test.flac https://www.openslr.org/resources/12/train-clean-100.tar.gz
# Extract one FLAC file...

mosqueeze predict test.flac
# Expected: Skip compression

# Test with PDF
wget -O test.pdf https://www.w3.org/WAI/WCGL/WCAG21/pdf.pdf
mosqueeze predict test.pdf
# Expected: Skip compression

# Test force override
mosqueeze compress test.flac --force -a zstd -l 3 -o out.flac
# Expected: Compressed (but verify ratio ~1.10x)
```

---

## Edge Cases

1. **FLAC with low entropy**: Rare, but possible in pathological cases
   - If entropy < 7.5, do NOT skip (may be rare uncompressed FLAC)
   
2. **PDF with embedded uncompressed streams**: Some PDFs have uncompressed content
   - Decision should be per-stream, but we skip entire file for now
   - Future: PDF preprocessor can extract/compress streams
   
3. **Force override**: User explicitly requests compression
   - `--force` flag bypasses skip logic

---

## Future Enhancements

### Per-Stream PDF Analysis (Future Issue)

```cpp
// Future: PDF preprocessor
class PDFPreprocessor : public IPreprocessor {
    // Extract streams, analyze each, recompress if beneficial
    std::vector<uint8_t> preprocess(const std::vector<uint8_t>& data) override;
};
```

### Audio Format Detection (Future Issue)

- Detect uncompressed audio (RAW, WAV) → compress
- Detect already-compressed audio (FLAC, MP3, AAC) → skip

---

## Migration Notes

- **Breaking change**: None — this adds skip logic, doesn't change existing behavior for compressible types
- **Configuration**: `selection_rules.json` new entries are additive
- **Decision matrix**: FLAC/PDF entries are new, not replacing existing

---

## Rollback Plan

If issues arise:
1. Remove FLAC/PDF from `skip_compression.extensions` in config
2. Set `entropyThreshold = 99.0` to effectively disable entropy check
3. Rebuild — all files will be eligible for compression

---

## References

- Porter benchmark data: `/home/moreno/mosqueeze-bench/decision_runs/consolidated-benchmark.db`
- Issue #109: PDF Document Type Support
- Porter benchmark report: `docs/benchmarks/decision-matrix-2026-04.md` (Porter section)
- Benchmark JSON: `docs/benchmarks/combined_decision_matrix.json`

---

## Notes for Implementer

- **Priority**: P2 — Performance optimization, not a bug
- **Complexity**: Medium — requires changes across multiple files
- **Testing**: Focus on skip logic and entropy threshold
- **Config-driven**: Skip list should be configurable, not hardcoded
- **Logging**: Add DEBUG logs for skip decisions (helps troubleshooting)
- **Force override**: Consider `--force-compression` CLI flag for edge cases

---

## Early Development Note

```
Breaking changes are acceptable. DB schema changes, table drops/recreates, 
sink wipes are OK during implementation. No backward compatibility needed.
```