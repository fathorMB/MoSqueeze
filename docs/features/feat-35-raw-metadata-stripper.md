# Worker Spec: Extend ImageMetaStripper to RAW Formats

**Issue:** #35  
**Branch:** `feat/benchmark-improvements`  
**Priority:** Medium  
**Type:** Enhancement

---

## Problem Statement

The current `ImageMetaStripper` preprocessor handles JPEG/PNG metadata but does not support RAW image formats which are TIFF-based containers:

| Format | Vendor | Container Type |
|--------|--------|----------------|
| `.raf` | Fujifilm | TIFF-based proprietary |
| `.nef` | Nikon | TIFF-EP |
| `.cr2`/`.cr3` | Canon | TIFF-based |
| `.dng` | Adobe | TIFF-EP (standard) |
| `.arw` | Sony | TIFF-based |
| `.orf` | Olympus | TIFF-based |

## Why This Matters

RAW files contain significant metadata that is highly compressible:
- EXIF block: ~50-200 KB
- Maker notes: Vendor-specific data
- Preview JPEGs: Often multiple sizes (thumbnail, preview)
- IPTC/XMP: Sidecar metadata

Stripping this metadata before compression can:
1. Remove ~50-200 KB per file
2. Enable better compression ratios on the actual Bayer sensor data
3. Preserve raw pixel data while reducing file size

---

## TIFF Container Structure

### Standard TIFF Structure
```
Offset 0:     TIFF Header (8 bytes)
              - Byte order: 'II' (little-endian) or 'MM' (big-endian)
              - Magic: 42
              - IFD0 offset

IFD0:         Image File Directory
              - Tag count
              - Tags (width, height, compression, strips, etc.)
              - Next IFD offset

Additional IFDs: Thumbnails, previews, EXIF, etc.
```

### RAW-Specific IFDs
```
IFD0:         Main image metadata
SubIFD:       Preview/thumbnail
EXIF IFD:     Camera settings
MakerNotes:   Vendor-specific (Nikon, Canon, etc.)
GPUrams:      Gamma correction tables
CFAPattern:   Bayer color filter array
```

---

## Implementation Plan

### Phase 1: FileTypeDetector Extension

**File:** `src/libmosqueeze/src/FileTypeDetector.cpp`

Add detection for TIFF-based RAW formats:

```cpp
// In detect() function, add TIFF-based detection
if (hasMagick(file, {0x49, 0x49, 0x2A, 0x00})) {  // "II*\0" - little-endian TIFF
    return classifyTiffBased(file);
}
if (hasMagick(file, {0x4D, 0x4D, 0x00, 0x2A})) {  // "MM\0*" - big-endian TIFF
    return classifyTiffBased(file);
}

// New helper function
FileType classifyTiffBased(const std::filesystem::path& file) {
    std::string ext = file.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".nef" || ext == ".nrw") return FileType::NikonRaw;
    if (ext == ".cr2" || ext == ".cr3") return FileType::CanonRaw;
    if (ext == ".raf") return FileType::FujifilmRaw;
    if (ext == ".arw" || ext == ".sr2") return FileType::SonyRaw;
    if (ext == ".dng") return FileType::DNG;
    if (ext == ".orf") return FileType::OlympusRaw;
    if (ext == ".rw2") return FileType::PanasonicRaw;
    
    return FileType::Tiff;  // Generic TIFF
}
```

**File:** `src/libmosqueeze/include/mosqueeze/FileTypeDetector.hpp`

Add new FileType enum values:
```cpp
enum class FileType {
    Unknown,
    Jpeg,
    Png,
    Tiff,
    // RAW formats
    NikonRaw,
    CanonRaw,
    FujifilmRaw,
    SonyRaw,
    DNG,
    OlympusRaw,
    PanasonicRaw,
    // ... existing types ...
};
```

### Phase 2: ImageMetaStripper Extension

**File:** `src/libmosqueeze/src/preprocessors/ImageMetaStripper.cpp`

Add TIFF/RAW metadata stripping:

```cpp
std::vector<uint8_t> ImageMetaStripper::process(const std::vector<uint8_t>& data, FileType type) {
    switch (type) {
        case FileType::Jpeg:
        case FileType::Png:
            return stripImageMetadata(data, type);
        
        case FileType::NikonRaw:
        case FileType::CanonRaw:
        case FileType::FujifilmRaw:
        case FileType::DNG:
        case FileType::SonyRaw:
        case FileType::OlympusRaw:
        case FileType::PanasonicRaw:
            return stripRawMetadata(data, type);
        
        default:
            return data;  // Pass through unsupported types
    }
}

std::vector<uint8_t> ImageMetaStripper::stripRawMetadata(
    const std::vector<uint8_t>& data, 
    FileType type
) {
    // Parse TIFF header
    if (data.size() < 8) return data;
    
    bool littleEndian = (data[0] == 'I' && data[1] == 'I');
    uint32_t ifdOffset = readU32(data, 4, littleEndian);
    
    // Build list of essential tags to preserve
    std::set<uint16_t> preserveTags = {
        256, 257, 258, 259,  // Width, Height, BitsPerSample, Compression
        273, 277, 278, 279,  // StripOffsets, SamplesPerPixel, RowsPerStrip, StripByteCounts
        339,                 // SampleFormat
        // RAW-specific
        0x828D,              // CFARepeatPatternDim
        0x828E,              // CFAPattern
        0x828F,              // ExposureTime
        // ... essential RAW tags
    };
    
    // Remove tags from preserve set:
    // - EXIF IFD (tag 34665)
    // - GPS IFD (tag 34853)
    // - MakerNotes (inside EXIF)
    // - Interop IFD (tag 40965)
    // - Preview IFDs
    
    // Rebuild TIFF with stripped metadata
    // ...
}
```

### Phase 3: TIFF Parser Helper

**File:** `src/libmosqueeze/src/preprocessors/TiffParser.hpp` (new file)

```cpp
namespace mosqueeze {

struct TiffTag {
    uint16_t tagId;
    uint16_t type;
    uint32_t count;
    uint32_t valueOrOffset;
};

class TiffParser {
public:
    explicit TiffParser(const std::vector<uint8_t>& data);
    
    bool isValid() const;
    bool isLittleEndian() const { return littleEndian_; }
    
    std::vector<TiffTag> readIfd(uint32_t offset);
    uint32_t readU32(uint32_t offset) const;
    uint16_t readU16(uint32_t offset) const;
    
    // Find IFD by tag ID
    std::optional<uint32_t> findIfdOffset(uint16_t ifdTag);
    
    // Get tag value
    std::optional<TiffTag> getTag(uint16_t tagId, uint32_t ifdOffset);
    
    // Strip specific tags and rebuild
    std::vector<uint8_t> stripTags(
        const std::set<uint16_t>& tagsToRemove,
        const std::set<uint16_t>& ifdsToRemove
    );
    
private:
    const std::vector<uint8_t>& data_;
    bool littleEndian_;
};

} // namespace mosqueeze
```

---

## Files to Modify

| File | Change |
|------|--------|
| `src/libmosqueeze/include/mosqueeze/FileTypeDetector.hpp` | Add RAW FileType enums |
| `src/libmosqueeze/src/FileTypeDetector.cpp` | Add TIFF/RAW detection |
| `src/libmosqueeze/src/preprocessors/ImageMetaStripper.cpp` | Add RAW stripping logic |
| `src/libmosqueeze/src/preprocessors/TiffParser.hpp` | TIFF parsing utilities |
| `src/libmosqueeze/src/preprocessors/TiffParser.cpp` | TIFF parsing implementation |
| `tests/unit/ImageMetaStripper_test.cpp` | Add RAW test cases |

---

## Testing Plan

### Unit Tests

```cpp
TEST(ImageMetaStripper, DetectsNefFiles) {
    FileTypeDetector detector;
    auto result = detector.detect("test.nef");
    EXPECT_EQ(result.type, FileType::NikonRaw);
}

TEST(ImageMetaStripper, DetectsRafFiles) {
    FileTypeDetector detector;
    auto result = detector.detect("test.raf");
    EXPECT_EQ(result.type, FileType::FujifilmRaw);
}

TEST(ImageMetaStripper, DetectsDngFiles) {
    FileTypeDetector detector;
    auto result = detector.detect("test.dng");
    EXPECT_EQ(result.type, FileType::DNG);
}

TEST(ImageMetaStripper, StripsNefMetadata) {
    std::vector<uint8_t> nefData = loadTestFile("test.nef");
    ImageMetaStripper stripper;
    auto result = stripper.process(nefData, FileType::NikonRaw);
    
    EXPECT_LT(result.size(), nefData.size());  // Should be smaller
    // Verify original size preserved (Bayer data)
}

TEST(TiffParser, ReadsIfd) {
    TiffParser parser(loadTestFile("test.nef"));
    EXPECT_TRUE(parser.isValid());
    
    auto tags = parser.readIfd(8);  // IFD0 typically at offset 8
    EXPECT_FALSE(tags.empty());
}

TEST(TiffParser, StripsExifIfd) {
    // Verify EXIF IFD is removable
}
```

### Integration Tests

```bash
# Test with actual RAW files
mosqueeze-bench -f "test.nef" --default-only -v -o "./results"
mosqueeze-bench -f "test.raf" --default-only -v -o "./results"
mosqueeze-bench -f "test.dng" --default-only -v -o "./results"
```

### Test Data Requirements

Need sample RAW files for testing:
- `test.nef` — Nikon NEF sample (can use public test images)
- `test.raf` — Fujifilm RAF sample
- `test.dng` — Adobe DNG sample
- `test.cr2` — Canon CR2 sample

---

## Implementation Phases

### Phase 1: Detection (1 day)
- Add FileType enums for RAW formats
- Implement TIFF magic number detection
- Verify file extension correlation

### Phase 2: TIFF Parsing (2 days)
- Implement TiffParser class
- Handle both endianness
- Read/write IFD structures

### Phase 3: Metadata Stripping (2 days)
- Identify non-essential IFDs (EXIF, GPS, Preview, MakerNotes)
- Implement strip logic
- Rebuild TIFF structure

### Phase 4: Testing (1 day)
- Unit tests for each format
- Roundtrip verification
- Compression comparison

---

## Verification Checklist

- [ ] FileTypeDetector correctly identifies NEF, RAF, CR2, DNG
- [ ] ImageMetaStripper processes RAW files without error
- [ ] Stripped files are smaller than originals
- [ ] Bayer data is preserved (image data intact)
- [ ] Roundtrip: original → strip → compress → decompress → restore
- [ ] Tests pass for all supported RAW formats

---

## Dependencies

- PR #32 (Preprocessing Engine) — already merged

---

## Risks

1. **Vendor-specific variations:** Each RAW format has proprietary extensions
   - Mitigation: Start with DNG (standardized), then add vendor-specific

2. **Metadata preservation:** Some metadata may be needed for image processing
   - Mitigation: Make stripping configurable (keep EXIF, remove previews, etc.)

3. **File corruption risk:** Incorrect TIFF editing can render files unreadable
   - Mitigation: Extensive testing, roundtrip verification

---

## Estimated Effort

| Task | Time |
|------|------|
| Phase 1: Detection | 1 day |
| Phase 2: TIFF Parser | 2 days |
| Phase 3: Stripping | 2 days |
| Phase 4: Testing | 1 day |
| **Total** | **6 days** |

---

## References

- [TIFF 6.0 Specification](https://www.adobe.io/open/document-format/pdf/1658705319505/TIFF-6.0.pdf)
- [TIFF/EP Specification](https://www.iso.org/standard/41876.html)
- [DNG Specification](https://www.adobe.io/content/dam/dw/1658705319505/dng/dng_spec_1.6.pdf)
- [ExifTool RAW format documentation](https://exiftool.org/TagNames/)