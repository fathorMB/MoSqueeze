# Worker Spec: PDF Document Type Support

**Issue**: [#109](https://github.com/fathorMB/MoSqueeze/issues/109)  
**Type**: Bug Fix + Enhancement  
**Priority**: P2-Medium  
**Severity**: Major  
**Estimated Effort**: 1-2 hours

---

## Summary

PDF files are detected via magic bytes but incorrectly mapped to `FileType::Binary_Chunked` instead of a dedicated `Document_PDF` type. This prevents PDF-specific preprocessing and results in suboptimal compression decisions.

---

## Problem Statement

### Current Behavior

```cpp
// FileTypeDetector.cpp:344
{{0x25, 0x50, 0x44, 0x46}, 0, FileType::Binary_Chunked, "application/pdf", true, true},
```

**Issues:**

| Problem | Impact |
|---------|--------|
| No `FileType::Document_PDF` enum | PDF = generic binary |
| No `.pdf` extension mapping | Magic-only detection |
| No PDF preprocessor selection | `preprocess_type: none` always |
| Decision matrix uses `Binary_Chunked` | Wrong compression coefficients |

### Evidence from Benchmark

```
synthetic-000.pdf | zstd/1 | ratio 3533.305x | encode 0ms | preprocess none
```

- Artificial ratio (3500x) due to synthetic repetitive content
- `preprocess: none` — should have PDF-specific preprocessing
- Not comparable to formats that have preprocessing (FLAC, WAV, etc.)

---

## Solution

### Design Decision

Add dedicated `Document_PDF` file type and update detection/preprocessing chain.

### Implementation

#### 1. Add FileType Enum

```cpp
// src/libmosqueeze/include/mosqueeze/Types.hpp

enum class FileType {
    // ... existing types ...
    
    // Documents (NEW)
    Document_PDF,
    Document_Office,  // Future: docx, xlsx, pptx
    
    // ... existing ...
};
```

#### 2. Update FileTypeDetector

```cpp
// src/libmosqueeze/src/FileTypeDetector.cpp

// Magic entry (line ~344)
{{0x25, 0x50, 0x44, 0x46}, 0, FileType::Document_PDF, "application/pdf", true, true},
//     ^^^^ magic: %PDF                ^^^^ NEW TYPE

// Extension map (add to extensionMap in detectFromExtension)
{".pdf", makeClassification(FileType::Document_PDF, "application/pdf", true, true, ".pdf")},
```

**Note:** PDF files are already compressed internally (often with FlateDecode/zlib), so:
- `isCompressed = true` — PDF is often already compressed
- `canRecompress = true` — But recompression can still help for uncompressed streams

#### 3. Update PreprocessorSelector (Optional - Future)

```cpp
// src/libmosqueeze/src/PreprocessorSelector.cpp (if exists)

IPreprocessor* PreprocessorSelector::selectBest(FileType type) const {
    switch (type) {
        // ... existing cases ...
        case FileType::Document_PDF:
            // Future: return pdfPreprocessor_.get();
            return nullptr;  // No PDF preprocessor yet
        // ...
    }
}
```

For now, no PDF preprocessor exists — `null` means `preprocess_type: none`.

#### 4. Add Decision Matrix Entry (Optional)

```cpp
// src/libmosqueeze/src/DecisionMatrix.cpp

// Add PDF-specific coefficients (if matrix supports it)
{"application/pdf", {
    .algorithm = "zstd",
    .level = 3,
    .preprocessing = "none",
    .expectedRatio = 1.05,  // Minimal gain (already compressed)
    .notes = "PDF internal compression often sufficient"
}},
```

---

## Files to Modify

| File | Changes | Lines |
|------|---------|-------|
| `Types.hpp` | Add `Document_PDF` to enum | ~1 line |
| `FileTypeDetector.cpp` | Update magic entry | 1 line |
| `FileTypeDetector.cpp` | Add `.pdf` extension | 1 line |
| `DecisionMatrix.cpp` | Add PDF entry (optional) | ~5 lines |

**Total: ~5-10 lines of code changes**

---

## Acceptance Criteria

### Must Have

1. **PDF detected as `Document_PDF`**
   ```bash
   printf '%%PDF-1.4\n%%EOF' > test.pdf
   mosqueeze-bench -d /tmp -a zstd -l 1 -o /tmp/out --verbose
   
   # Check output
   grep "detected_type.*pdf\|Document_PDF" /tmp/out/results.json
   ```

2. **`.pdf` extension recognized**
   ```bash
   # File without magic bytes but .pdf extension
   echo "dummy" > /tmp/fake.pdf
   mosqueeze-bench -d /tmp/fake.pdf -a zstd -l 1 -o /tmp/out
   
   # Should still detect as Document_PDF (by extension)
   ```

3. **MIME preserved**
   ```bash
   sqlite3 /tmp/out/results.sqlite3 "SELECT DISTINCT detected_type FROM benchmark_results WHERE file LIKE '%.pdf';"
   # Expected: application/pdf or Document_PDF
   ```

### Should Have

4. **Decision matrix entry for PDF**
   ```bash
   # Check decision matrix includes pdf
   mosqueeze predict test.pdf
   # Should return algorithm/level for Document_PDF
   ```

5. **Unit tests pass**
   ```bash
   cd build && ctest -R FileTypeDetector
   ```

---

## Testing Plan

### Unit Tests

Create or update `tests/unit/FileTypeDetector_test.cpp`:

```cpp
TEST(FileTypeDetector, DetectsPDFViaMagic) {
    FileTypeDetector detector;
    
    // Create PDF with magic bytes
    std::vector<uint8_t> buffer = {0x25, 0x50, 0x44, 0x46, ...};  // %PDF...
    
    auto result = detector.detectFromMagic(buffer);
    
    EXPECT_EQ(result.type, FileType::Document_PDF);
    EXPECT_EQ(result.mimeType, "application/pdf");
}

TEST(FileTypeDetector, DetectsPDFViaExtension) {
    FileTypeDetector detector;
    
    auto result = detector.detectFromExtension(".pdf");
    
    EXPECT_EQ(result.type, FileType::Document_PDF);
}

TEST(FileTypeDetector, PDFIsAlreadyCompressed) {
    FileTypeDetector detector;
    auto result = detector.detectFromExtension(".pdf");
    
    EXPECT_TRUE(result.isCompressed);
}
```

### Integration Tests

```bash
# Test with real PDF
wget -O /tmp/test.pdf https://www.w3.org/WAI/WCGL/WCAG21/pdf.pdf
mosqueeze-bench -d /tmp/test.pdf -a zstd -l 1,3,6,9 -o /tmp/out

# Verify type
sqlite3 /tmp/out/results.sqlite3 "SELECT algorithm, level, ratio FROM benchmark_results WHERE file LIKE '%test.pdf';"
```

---

## Edge Cases

1. **Empty PDF**: Should still be detected by magic bytes
2. **Corrupted PDF**: Magic bytes present but content invalid → still `Document_PDF`
3. **PDF-like binary**: `%PDF` at offset 0 → detected as PDF (correct behavior)
4. **PDF without extension**: Magic detection should still work

---

## Future Enhancements

### PDF Preprocessor (Future Issue)

Potential preprocessing for PDF:
- `pdf-linearize` — Linearize for faster web viewing
- `pdf-extract-text` — Extract text before compression
- `pdf-strip-metadata` — Remove XMP/metadata
- `pdf-recompress-streams` — Recompress internal streams

For now: `preprocess_type: none` is acceptable.

### Office Documents (Future)

Add similar detection for:
- `.docx` → `Document_Office` (ZIP-based)
- `.xlsx` → `Document_Office`
- `.pptx` → `Document_Office`

---

## Migration Notes

- **Breaking change**: None — new enum value doesn't affect existing code
- **Decision matrix**: May need update if PDF-specific logic exists
- **Preprocessor**: N/A — no PDF preprocessor exists yet

---

## Rollback Plan

If issues arise:
1. Revert `FileType::Document_PDF` to `FileType::Binary_Chunked`
2. PDF behavior returns to original

---

## References

- Original bug report: [Issue #109](https://github.com/fathorMB/MoSqueeze/issues/109)
- Porter's PDF benchmark: `~/Desktop/workspace/audio-baseline/pdf-corpus/`
- PDF magic bytes: `0x25 0x50 0x44 0x46` = `%PDF`

---

## Notes for Implementer

- Simple fix: ~5 lines of code
- No preprocessor implementation needed (yet)
- Decision matrix update is optional but recommended
- Add unit tests for `Document_PDF` type
- Test with both magic-detection and extension-detection paths