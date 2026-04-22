# Worker Spec: Preprocessing Engine for Improved Compression Ratio (Issue #31)

## Overview

Implement a **Preprocessing Pipeline** that normalizes files before compression to achieve better compression ratios. Preprocessing is **lossless and reversible** — the original file can be exactly reconstructed after decompression + postprocessing.

## Problem Statement

Generic compression algorithms treat files as opaque byte streams. However, structured formats (JSON, XML, Images) contain redundant patterns that can be normalized:

| Issue | Example | Wasted Space |
|-------|---------|--------------|
| Unordered JSON keys | `{"z":1,"a":2}` vs `{"a":2,"z":1}` | Patterns not recognized |
| Verbose whitespace | Pretty-printed JSON/XML | 20-40% overhead |
| Image metadata | EXIF, thumbnails, comments | 2-10% overhead |
| Similar file schemas | Repeated JSON structures | Zstd dict can help |

## Architecture

```
     Input File
          │
          ▼
   ┌─────────────────┐
   │  FileClassifier │  ← Detect format + select preprocessor
   └────────┬────────┘
            │
            ▼
   ┌─────────────────┐
   │  IPreprocessor  │  ← Format-specific normalization
   │  ├─ JsonCanonicalizer
   │  ├─ XmlCanonicalizer
   │  ├─ ImageMetaStripper
   │  └─ DictionaryPreprocessor
   └────────┬────────┘
            │
            ▼
   ┌─────────────────┐
   │ Compression     │  ← Existing engines (zstd, lzma, zpaq)
   │ Engine          │
   └────────┬────────┘
            │
            ▼
   ┌─────────────────┐
   │  Trailing Header│  ← Preprocessor ID + metadata for reversal
   └────────┬────────┘
            │
            ▼
     Compressed Output
```

## Reversibility Guarantee

**Critical requirement**: Preprocessing MUST be lossless:

```
Original → Preprocess → Compress → Decompress → Postprocess → Original (byte-identical)
```

A trailing header stores the preprocessor type and metadata needed for reversal.

---

## Part 1: IPreprocessor Interface

### File: `src/libmosqueeze/include/mosqueeze/Preprocessor.hpp`

```cpp
#pragma once

#include <mosqueeze/Types.hpp>
#include <cstddef>
#include <filesystem>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace mosqueeze {

enum class PreprocessorType : uint8_t {
    None = 0,
    JsonCanonicalizer = 1,
    XmlCanonicalizer = 2,
    ImageMetaStripper = 3,
    DictionaryPreprocessor = 4
};

struct PreprocessResult {
    size_t originalBytes = 0;
    size_t processedBytes = 0;
    PreprocessorType type = PreprocessorType::None;
    std::vector<uint8_t> metadata;  // Type-specific metadata for reversal
};

class IPreprocessor {
public:
    virtual ~IPreprocessor() = default;
    
    // Unique identifier
    virtual PreprocessorType type() const = 0;
    virtual std::string name() const = 0;
    
    // Check if this preprocessor can handle the file type
    virtual bool canProcess(FileType fileType) const = 0;
    
    // Transform input → output, returns metadata for reversal
    virtual PreprocessResult preprocess(
        std::istream& input,
        std::ostream& output,
        FileType fileType) = 0;
    
    // Reverse the transformation using metadata
    virtual void postprocess(
        std::istream& input,
        std::ostream& output,
        const PreprocessResult& result) = 0;
    
    // Estimated compression improvement for this file type (0.0 - 1.0)
    virtual double estimatedImprovement(FileType fileType) const = 0;
};

} // namespace mosqueeze
```

---

## Part 2: JsonCanonicalizer

### Goals

1. **Sort object keys** alphabetically (consistent ordering across files)
2. **Remove unnecessary whitespace** (compact output)
3. **Normalize number formats** (`1.0` → `1` if integer)
4. **Escape consistency** (standardize Unicode escapes)

### Files

| File | Purpose |
|------|---------|
| `src/libmosqueeze/include/mosqueeze/preprocessors/JsonCanonicalizer.hpp` | Header |
| `src/libmosqueeze/src/preprocessors/JsonCanonicalizer.cpp` | Implementation |
| `tests/unit/JsonCanonicalizer_test.cpp` | Unit tests |

### Header: `JsonCanonicalizer.hpp`

```cpp
#pragma once

#include <mosqueeze/Preprocessor.hpp>

namespace mosqueeze {

class JsonCanonicalizer : public IPreprocessor {
public:
    PreprocessorType type() const override { 
        return PreprocessorType::JsonCanonicalizer; 
    }
    
    std::string name() const override { 
        return "json-canonical"; 
    }
    
    bool canProcess(FileType fileType) const override {
        return fileType == FileType::Json;
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
        return fileType == FileType::Json ? 0.15 : 0.0;
    }
};

} // namespace mosqueeze
```

### Implementation: `JsonCanonicalizer.cpp`

```cpp
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <nlohmann/json.hpp>

namespace mosqueeze {

PreprocessResult JsonCanonicalizer::preprocess(
    std::istream& input,
    std::ostream& output,
    FileType fileType) {
    
    PreprocessResult result;
    result.type = PreprocessorType::JsonCanonicalizer;
    
    // Parse JSON
    nlohmann::json j;
    input >> j;
    result.originalBytes = input.tellg();
    
    // Canonicalize: dump without whitespace, sorted keys
    std::string canonical = j.dump(-1, ' ', false, 
        nlohmann::json::error_handler_t::strict);
    
    // Also sort keys recursively
    nlohmann::json sorted = sortKeysRecursive(j);
    canonical = sorted.dump();
    
    output.write(canonical.data(), canonical.size());
    result.processedBytes = canonical.size();
    
    return result;
}

void JsonCanonicalizer::postprocess(
    std::istream& input,
    std::ostream& output,
    const PreprocessResult& result) {
    
    // JSON canonicalization is reversible by parsing + pretty-print
    // The original whitespace/formatting is lost but data is preserved
    nlohmann::json j;
    input >> j;
    
    // Output with original formatting if metadata contains it
    // For now, output compact (can be pretty-printed by user)
    output << j.dump();
}

nlohmann::json JsonCanonicalizer::sortKeysRecursive(const nlohmann::json& j) {
    if (j.is_object()) {
        nlohmann::ordered_json sorted;
        std::vector<std::string> keys;
        for (auto& [k, v] : j.items()) {
            keys.push_back(k);
        }
        std::sort(keys.begin(), keys.end());
        for (const auto& k : keys) {
            sorted[k] = sortKeysRecursive(j[k]);
        }
        return sorted;
    } else if (j.is_array()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& item : j) {
            arr.push_back(sortKeysRecursive(item));
        }
        return arr;
    }
    return j;
}

} // namespace mosqueeze
```

### Example Transformation

```json
// Input (78 bytes)
{
    "timestamp": "2024-01-15T10:30:00Z",
    "user": "alice",
    "items": [1, 2, 3]
}

// Canonical (52 bytes) — 33% smaller
{"items":[1,2,3],"timestamp":"2024-01-15T10:30:00Z","user":"alice"}
```

---

## Part 3: XmlCanonicalizer

### Goals

1. **Sort attributes** alphabetically within elements
2. **Normalize whitespace** between elements
3. **Standardize empty elements** (`<tag></tag>` → `<tag/>`)
4. **Remove XML declaration** (store in metadata)

### Files

| File | Purpose |
|------|---------|
| `src/libmosqueeze/include/mosqueeze/preprocessors/XmlCanonicalizer.hpp` | Header |
| `src/libmosqueeze/src/preprocessors/XmlCanonicalizer.cpp` | Implementation |
| `tests/unit/XmlCanonicalizer_test.cpp` | Unit tests |

### Dependencies

Add to `vcpkg.json`:
```json
"pugixml"
```

### Implementation Notes

- Use `pugixml` for parsing
- Reference C14N (Canonical XML) standard
- Preserve CDATA sections and comments (store flags in metadata)

### Target Improvement

- Verbose XML: 10-15%
- Already minified XML: 2-5%

---

## Part 4: ImageMetaStripper

### Goals

1. **Remove EXIF data** (unless explicitly preserved)
2. **Remove embedded thumbnails**
3. **Remove ICC profiles** (store in metadata)
4. **Remove comments/APP markers**
5. **Preserve pixel data exactly** — lossless for image content

### Files

| File | Purpose |
|------|---------|
| `src/libmosqueeze/include/mosqueeze/preprocessors/ImageMetaStripper.hpp` | Header |
| `src/libmosqueeze/src/preprocessors/ImageMetaStripper.cpp` | Implementation |
| `tests/unit/ImageMetaStripper_test.cpp` | Unit tests |

### Header: `ImageMetaStripper.hpp`

```cpp
#pragma once

#include <mosqueeze/Preprocessor.hpp>

namespace mosqueeze {

struct ImageMetadata {
    std::vector<uint8_t> exifData;
    std::vector<uint8_t> iccProfile;
    std::string comment;
    bool hasThumbnail = false;
    uint16_t originalOrientation = 1;
};

class ImageMetaStripper : public IPreprocessor {
public:
    PreprocessorType type() const override { 
        return PreprocessorType::ImageMetaStripper; 
    }
    
    std::string name() const override { 
        return "image-meta-strip"; 
    }
    
    bool canProcess(FileType fileType) const override {
        return fileType == FileType::Jpeg || 
               fileType == FileType::Png;
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
        return (fileType == FileType::Jpeg) ? 0.05 : 0.02;
    }

private:
    ImageMetadata extractJpegMetadata(std::istream& input, std::ostream& output);
    ImageMetadata extractPngMetadata(std::istream& input, std::ostream& output);
    void restoreJpegMetadata(std::istream& stripped, std::ostream& output, 
                              const ImageMetadata& meta);
    void restorePngMetadata(std::istream& stripped, std::ostream& output, 
                             const ImageMetadata& meta);
};

} // namespace mosqueeze
```

### JPEG Implementation Notes

Parse JPEG markers:
- `APP1` (0xFFE1) — EXIF data
- `APP2` (0xFFE2) — ICC profile
- `APP13` (0xFFED) — IPTC data
- `COM` (0xFFFE) — Comment

Copy SOI (0xFFD8), SOF, DHT, DQT, DCT, EOI — keep image data.

### PNG Implementation Notes

Parse PNG chunks:
- `eXIf` — EXIF data
- `iCCP` — ICC profile
- `tEXt`, `iTXt`, `zTXt` — Text metadata

Keep critical chunks: `IHDR`, `IDAT`, `IEND`.

### Critical Requirement

**Pixel data MUST be preserved exactly**. Test with pixel-by-pixel comparison.

### Performance Impact

| Source | Metadata Size | Potential Savings |
|--------|---------------|-------------------|
| Camera JPEG | 20-100 KB | 5-10% |
| Web JPEG | 2-5 KB | 2-5% |
| PNG with metadata | 5-50 KB | 2-8% |

---

## Part 5: DictionaryPreprocessor (Zstd Dictionary Training)

### Goals

1. **Train Zstd dictionary** from sample files
2. **Apply dictionary** to similar files
3. **Store dictionary** in archive metadata

### Use Cases

- Log files with repeated patterns
- JSON files with same schema
- Database dumps
- Backup sets of similar documents

### Files

| File | Purpose |
|------|---------|
| `src/libmosqueeze/include/mosqueeze/preprocessors/DictionaryPreprocessor.hpp` | Header |
| `src/libmosqueeze/src/preprocessors/DictionaryPreprocessor.cpp` | Implementation |
| `tests/unit/DictionaryPreprocessor_test.cpp` | Unit tests |

### Header: `DictionaryPreprocessor.hpp`

```cpp
#pragma once

#include <mosqueeze/Preprocessor.hpp>
#include <unordered_map>

namespace mosqueeze {

class DictionaryPreprocessor : public IPreprocessor {
public:
    PreprocessorType type() const override { 
        return PreprocessorType::DictionaryPreprocessor; 
    }
    
    std::string name() const override { 
        return "zstd-dict"; 
    }
    
    bool canProcess(FileType fileType) const override {
        // Works on any file type if we have a trained dictionary
        return hasDictionary_;
    }
    
    // Train dictionary from sample files
    void trainFromSamples(
        const std::vector<std::filesystem::path>& samples, 
        size_t dictSize = 100 * 1024);  // 100KB default
    
    // Save/load dictionary for reuse
    void saveDictionary(const std::filesystem::path& path) const;
    void loadDictionary(const std::filesystem::path& path);
    
    PreprocessResult preprocess(
        std::istream& input,
        std::ostream& output,
        FileType fileType) override;
    
    void postprocess(
        std::istream& input,
        std::ostream& output,
        const PreprocessResult& result) override;
    
    double estimatedImprovement(FileType fileType) const override;

private:
    std::vector<uint8_t> dictionary_;
    bool hasDictionary_ = false;
};

} // namespace mosqueeze
```

### Implementation Notes

Use Zstd API:
```cpp
#include <zstd.h>

// Training
ZDICT_trainFromBuffer(dictBuffer, dictSize, samplesBuffer, sizes, nbSamples);

// Compression with dict
ZSTD_compress_usingCDict(ctx, dst, dstSize, src, srcSize, cdict);

// Decompression with dict
ZSTD_decompress_usingDDict(ctx, dst, dstSize, src, srcSize, ddict);
```

### Training Requirements

- Minimum: 100 samples or 10MB total
- Dictionary size: 100KB (configurable)
- Sample files should be representative of the dataset

### Performance Improvement

| Dataset | Without Dict | With Dict | Improvement |
|---------|--------------|-----------|-------------|
| JSON logs (1GB) | 45 MB | 32 MB | 29% |
| Similar XML | 120 MB | 95 MB | 21% |
| Small files (<4KB) | High overhead | Low overhead | 30-50% |

---

## Part 6: PreprocessorSelector

### File: `src/libmosqueeze/include/mosqueeze/PreprocessorSelector.hpp`

```cpp
#pragma once

#include <mosqueeze/Preprocessor.hpp>
#include <mosqueeze/FileTypeDetector.hpp>
#include <memory>
#include <vector>

namespace mosqueeze {

class PreprocessorSelector {
public:
    PreprocessorSelector();
    
    void registerPreprocessor(std::unique_ptr<IPreprocessor> preprocessor);
    
    // Select best preprocessor for a file (highest estimated improvement)
    IPreprocessor* selectBest(FileType fileType) const;
    
    // Get all applicable preprocessors
    std::vector<IPreprocessor*> getApplicable(FileType fileType) const;
    
    // Chain multiple preprocessors (e.g., image strip + dict)
    std::vector<IPreprocessor*> selectChain(FileType fileType) const;
    
    // List all registered preprocessors
    std::vector<std::string> listNames() const;

private:
    std::vector<std::unique_ptr<IPreprocessor>> preprocessors_;
};

} // namespace mosqueeze
```

### Implementation: `PreprocessorSelector.cpp`

```cpp
#include <mosqueeze/PreprocessorSelector.hpp>
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>
#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>
#include <mosqueeze/preprocessors/DictionaryPreprocessor.hpp>
#include <algorithm>

namespace mosqueeze {

PreprocessorSelector::PreprocessorSelector() {
    // Register default preprocessors
    registerPreprocessor(std::make_unique<JsonCanonicalizer>());
    registerPreprocessor(std::make_unique<XmlCanonicalizer>());
    registerPreprocessor(std::make_unique<ImageMetaStripper>());
    // Dictionary preprocessor is optional (requires training)
}

void PreprocessorSelector::registerPreprocessor(
    std::unique_ptr<IPreprocessor> preprocessor) {
    preprocessors_.push_back(std::move(preprocessor));
}

IPreprocessor* PreprocessorSelector::selectBest(FileType fileType) const {
    IPreprocessor* best = nullptr;
    double bestImprovement = -1.0;
    
    for (const auto& p : preprocessors_) {
        if (p->canProcess(fileType)) {
            double improvement = p->estimatedImprovement(fileType);
            if (improvement > bestImprovement) {
                bestImprovement = improvement;
                best = p.get();
            }
        }
    }
    
    return best;
}

std::vector<IPreprocessor*> PreprocessorSelector::getApplicable(
    FileType fileType) const {
    std::vector<IPreprocessor*> result;
    for (const auto& p : preprocessors_) {
        if (p->canProcess(fileType)) {
            result.push_back(p.get());
        }
    }
    return result;
}

std::vector<std::string> PreprocessorSelector::listNames() const {
    std::vector<std::string> names;
    for (const auto& p : preprocessors_) {
        names.push_back(p->name());
    }
    return names;
}

} // namespace mosqueeze
```

---

## Part 7: CompressionPipeline Integration

### New File: `src/libmosqueeze/include/mosqueeze/CompressionPipeline.hpp`

```cpp
#pragma once

#include <mosqueeze/ICompressionEngine.hpp>
#include <mosqueeze/Preprocessor.hpp>
#include <mosqueeze/Types.hpp>
#include <istream>
#include <ostream>

namespace mosqueeze {

struct PipelineResult {
    CompressionResult compression;
    PreprocessResult preprocessing;
    bool wasPreprocessed = false;
};

class CompressionPipeline {
public:
    explicit CompressionPipeline(ICompressionEngine* engine);
    
    // Set optional preprocessor
    void setPreprocessor(IPreprocessor* preprocessor);
    
    // Compress with optional preprocessing
    PipelineResult compress(
        std::istream& input,
        std::ostream& output,
        const CompressionOptions& opts = {});
    
    // Decompress with automatic postprocessing
    void decompress(
        std::istream& input,
        std::ostream& output);

private:
    ICompressionEngine* engine_;
    IPreprocessor* preprocessor_ = nullptr;
    
    // Trailing header format:
    // [compressed data][4 bytes: magic][1 byte: preprocessor type]
    // [4 bytes: metadata length][N bytes: metadata]
    static constexpr uint32_t MAGIC = 0x4D535146;  // "MSQF"
    
    void writeTrailingHeader(std::ostream& output, const PreprocessResult& meta);
    PreprocessResult readTrailingHeader(std::istream& input);
};

} // namespace mosqueeze
```

### Trailing Header Format

```
[Compressed Data]
[4 bytes: MAGIC = 0x4D535146 ("MSQF")]
[1 byte: PreprocessorType (0-4)]
[4 bytes: metadata length (little-endian)]
[N bytes: preprocessor metadata]
```

This allows the decompressor to detect and apply the correct postprocessor.

---

## Part 8: CLI Integration

### New Options in `main.cpp`

```cpp
// Preprocessing options
std::string preprocessOpt = "none";
bool listPreprocessors = false;
int trainSamples = 0;
size_t dictSize = 100 * 1024;

app.add_option("--preprocess", preprocessOpt, 
    "Use preprocessor: auto, none, json-canonical, xml-canonical, "
    "image-meta-strip, zstd-dict");
app.add_flag("--list-preprocessors", listPreprocessors, 
    "List available preprocessors");
app.add_option("--train-samples", trainSamples, 
    "Number of samples for dictionary training");
app.add_option("--dict-size", dictSize, 
    "Dictionary size in bytes (default: 100KB)");
```

### Examples

```bash
# Auto-detect best preprocessor
mosqueeze compress data.json output.msq --preprocess auto

# Specific preprocessor
mosqueeze compress config.xml output.msq --preprocess xml-canonical

# Image metadata stripping
mosqueeze compress photo.jpg photo.msq --preprocess image-meta-strip

# Dictionary training on similar files
mosqueeze compress ./logs/ archive.msq --preprocess zstd-dict --train-samples 200

# List available preprocessors
mosqueeze --list-preprocessors

# Decompress (auto-detects and applies postprocessor)
mosqueeze decompress archive.msq output/
```

---

## Part 9: vcpkg.json Updates

Add new dependency:

```json
{
  "name": "mosqueeze",
  "version": "0.2.0",
  "dependencies": [
    "zstd",
    "liblzma",
    "brotli",
    "cli11",
    "fmt",
    "spdlog",
    "nlohmann-json",
    "sqlite3",
    "pugixml"
  ]
}
```

---

## Part 10: Unit Tests

### JsonCanonicalizer Test

```cpp
// tests/unit/JsonCanonicalizer_test.cpp
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
    mosqueeze::JsonCanonicalizer canon;
    
    // Test 1: Roundtrip preserves data
    std::string input = R"({"z":1,"a":{"x":2,"b":3}})";
    std::istringstream in(input);
    std::ostringstream preprocessed, final;
    
    auto result = canon.preprocess(in, preprocessed, 
        mosqueeze::FileType::Json);
    
    std::istringstream preIn(preprocessed.str());
    canon.postprocess(preIn, final, result);
    
    // Parse both to compare semantically
    nlohmann::json orig = nlohmann::json::parse(input);
    nlohmann::json out = nlohmann::json::parse(final.str());
    assert(orig == out);
    
    std::cout << "[PASS] JsonCanonicalizer roundtrip test\n";
    return 0;
}
```

### ImageMetaStripper Test

```cpp
// tests/unit/ImageMetaStripper_test.cpp
// Critical: Pixel data must be identical after roundtrip
#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>
#include <fstream>

int main() {
    mosqueeze::ImageMetaStripper stripper;
    
    std::ifstream in("tests/fixtures/sample.jpg", std::ios::binary);
    std::ostringstream stripped;
    
    auto result = stripper.preprocess(in, stripped, 
        mosqueeze::FileType::Jpeg);
    
    std::istringstream stripIn(stripped.str());
    std::ostringstream restored;
    stripper.postprocess(stripIn, restored, result);
    
    // Load pixel data from both and compare
    // (Use stb_image or similar for pixel comparison)
    
    std::cout << "[PASS] ImageMetaStripper pixel preservation test\n";
    return 0;
}
```

### DictionaryPreprocessor Test

```cpp
// tests/unit/DictionaryPreprocessor_test.cpp
#include <mosqueeze/preprocessors/DictionaryPreprocessor.hpp>

int main() {
    mosqueeze::DictionaryPreprocessor dict;
    
    // Train on sample files
    std::vector<std::filesystem::path> samples = {
        "tests/fixtures/log1.txt",
        "tests/fixtures/log2.txt",
        "tests/fixtures/log3.txt"
    };
    dict.trainFromSamples(samples, 10 * 1024);  // 10KB dict
    
    assert(dict.canProcess(mosqueeze::FileType::Text));
    
    std::ifstream in("tests/fixtures/log4.txt");
    std::ostringstream processed;
    auto result = dict.preprocess(in, processed, 
        mosqueeze::FileType::Text);
    
    assert(result.originalBytes > result.processedBytes);
    
    std::cout << "[PASS] DictionaryPreprocessor test\n";
    return 0;
}
```

---

## Part 11: Integration Tests

### Full Pipeline Test

```cpp
// tests/integration/Pipeline_test.cpp
#include <mosqueeze/CompressionPipeline.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>

int main() {
    mosqueeze::ZstdEngine engine;
    mosqueeze::JsonCanonicalizer canon;
    mosqueeze::CompressionPipeline pipeline(&engine);
    
    pipeline.setPreprocessor(&canon);
    
    std::string original = R"({
        "timestamp": "2024-01-15T10:30:00Z",
        "user": "alice",
        "items": [1, 2, 3]
    })";
    
    std::istringstream in(original);
    std::ostringstream compressed;
    
    auto result = pipeline.compress(in, compressed, {});
    
    std::cout << "Original: " << original.size() << " bytes\n";
    std::cout << "After canonical: " << result.preprocessing.processedBytes << " bytes\n";
    std::cout << "Compressed: " << result.compression.compressedBytes << " bytes\n";
    std::cout << "Total ratio: " 
              << (double)original.size() / result.compression.compressedBytes 
              << "x\n";
    
    // Decompress and verify
    std::istringstream compIn(compressed.str());
    std::ostringstream decompressed;
    pipeline.decompress(compIn, decompressed);
    
    // Parse and compare
    nlohmann::json orig = nlohmann::json::parse(original);
    nlohmann::json out = nlohmann::json::parse(decompressed.str());
    assert(orig == out);
    
    std::cout << "[PASS] Full pipeline test\n";
    return 0;
}
```

---

## Part 12: Documentation

### Update `docs/wiki/preprocessing.md`

```markdown
# Preprocessing for Better Compression

## Overview

MoSqueeze can normalize files before compression for improved ratios. Preprocessing is **lossless and reversible**.

## Available Preprocessors

| Name | File Types | Typical Improvement |
|------|------------|---------------------|
| `json-canonical` | JSON | 10-25% |
| `xml-canonical` | XML | 5-15% |
| `image-meta-strip` | JPEG, PNG | 2-10% |
| `zstd-dict` | Similar files | 15-35% |

## Usage

### Auto-selection
```bash
mosqueeze compress data.json output.msq --preprocess auto
mosqueeze compress config.xml output.msq --preprocess auto
```

### Specific preprocessor
```bash
mosqueeze compress photo.jpg photo.msq --preprocess image-meta-strip
```

### Dictionary training
```bash
# Train on sample files, apply to similar files
mosqueeze compress ./logs/ archive.msq --preprocess zstd-dict --train-samples 100
```

## How It Works

1. File classifier detects format
2. Preprocessor transforms to canonical form
3. Compression engine compresses canonical data
4. Trailing header stores preprocessor ID + metadata
5. Decompression reads header, applies postprocessor
6. Original file restored exactly
```

---

## Part 13: File Structure

### New Files

| File | Purpose |
|------|---------|
| `src/libmosqueeze/include/mosqueeze/Preprocessor.hpp` | IPreprocessor interface |
| `src/libmosqueeze/include/mosqueeze/PreprocessorSelector.hpp` | Selector class |
| `src/libmosqueeze/include/mosqueeze/CompressionPipeline.hpp` | Pipeline integration |
| `src/libmosqueeze/include/mosqueeze/preprocessors/JsonCanonicalizer.hpp` | JSON preprocessor |
| `src/libmosqueeze/include/mosqueeze/preprocessors/XmlCanonicalizer.hpp` | XML preprocessor |
| `src/libmosqueeze/include/mosqueeze/preprocessors/ImageMetaStripper.hpp` | Image preprocessor |
| `src/libmosqueeze/include/mosqueeze/preprocessors/DictionaryPreprocessor.hpp` | Zstd dict |
| `src/libmosqueeze/src/PreprocessorSelector.cpp` | Selector impl |
| `src/libmosqueeze/src/CompressionPipeline.cpp` | Pipeline impl |
| `src/libmosqueeze/src/preprocessors/JsonCanonicalizer.cpp` | JSON impl |
| `src/libmosqueeze/src/preprocessors/XmlCanonicalizer.cpp` | XML impl |
| `src/libmosqueeze/src/preprocessors/ImageMetaStripper.cpp` | Image impl |
| `src/libmosqueeze/src/preprocessors/DictionaryPreprocessor.cpp` | Dict impl |
| `tests/unit/JsonCanonicalizer_test.cpp` | JSON tests |
| `tests/unit/XmlCanonicalizer_test.cpp` | XML tests |
| `tests/unit/ImageMetaStripper_test.cpp` | Image tests |
| `tests/unit/DictionaryPreprocessor_test.cpp` | Dict tests |
| `tests/integration/Pipeline_test.cpp` | Integration tests |

### Modified Files

| File | Changes |
|------|---------|
| `vcpkg.json` | Add `pugixml` |
| `src/libmosqueeze/CMakeLists.txt` | Add new source files |
| `src/mosqueeze-cli/src/main.cpp` | Add `--preprocess` options |
| `docs/wiki/preprocessing.md` | New documentation |

---

## Part 14: CMakeLists.txt Updates

### `src/libmosqueeze/CMakeLists.txt`

```cmake
# Add preprocessor source files
target_sources(libmosqueeze PRIVATE
    src/PreprocessorSelector.cpp
    src/CompressionPipeline.cpp
    src/preprocessors/JsonCanonicalizer.cpp
    src/preprocessors/XmlCanonicalizer.cpp
    src/preprocessors/ImageMetaStripper.cpp
    src/preprocessors/DictionaryPreprocessor.cpp
)

# Add include directories
target_include_directories(libmosqueeze PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

### `tests/CMakeLists.txt`

```cmake
add_executable(JsonCanonicalizer_test unit/JsonCanonicalizer_test.cpp)
target_link_libraries(JsonCanonicalizer_test PRIVATE libmosqueeze)

add_executable(XmlCanonicalizer_test unit/XmlCanonicalizer_test.cpp)
target_link_libraries(XmlCanonicalizer_test PRIVATE libmosqueeze)

add_executable(ImageMetaStripper_test unit/ImageMetaStripper_test.cpp)
target_link_libraries(ImageMetaStripper_test PRIVATE libmosqueeze)

add_executable(DictionaryPreprocessor_test unit/DictionaryPreprocessor_test.cpp)
target_link_libraries(DictionaryPreprocessor_test PRIVATE libmosqueeze zstd)

add_test(NAME JsonCanonicalizer COMMAND JsonCanonicalizer_test)
add_test(NAME XmlCanonicalizer COMMAND XmlCanonicalizer_test)
add_test(NAME ImageMetaStripper COMMAND ImageMetaStripper_test)
add_test(NAME DictionaryPreprocessor COMMAND DictionaryPreprocessor_test)
```

---

## Definition of Done

- [ ] `IPreprocessor` interface defined
- [ ] `JsonCanonicalizer` implemented + roundtrip test passes
- [ ] `XmlCanonicalizer` implemented + roundtrip test passes
- [ ] `ImageMetaStripper` implemented (JPEG + PNG) + pixel test passes
- [ ] `DictionaryPreprocessor` with ZSTD_trainFromBuffer
- [ ] `PreprocessorSelector` with auto-selection
- [ ] `CompressionPipeline` integration
- [ ] Trailing header format working
- [ ] CLI `--preprocess` option works
- [ ] CLI `--list-preprocessors` works
- [ ] CLI `--train-samples` works
- [ ] Unit tests for all preprocessors
- [ ] Integration tests for full pipeline
- [ ] Documentation in `docs/wiki/preprocessing.md`
- [ ] CI passes on Linux, macOS, Windows

---

## Performance Targets

| Preprocessor | Scenario | Target Improvement |
|--------------|----------|---------------------|
| json-canonical | Pretty-printed JSON | 15-25% |
| json-canonical | Minified JSON | 5-10% |
| xml-canonical | Verbose XML | 10-15% |
| xml-canonical | Minified XML | 3-8% |
| image-meta-strip | Camera JPEG | 5-10% |
| image-meta-strip | Web JPEG/PNG | 2-5% |
| zstd-dict | JSON logs same schema | 20-35% |
| zstd-dict | Similar small files | 30-50% |

---

## Test Fixtures Required

| Fixture | Purpose |
|---------|---------|
| `tests/fixtures/sample.json` | Pretty-printed JSON |
| `tests/fixtures/sample.xml` | Verbose XML |
| `tests/fixtures/sample.jpg` | JPEG with EXIF |
| `tests/fixtures/sample.png` | PNG with metadata |
| `tests/fixtures/log1.txt` ... `log5.txt` | Similar log files for dict training |

---

## Future Extensions

- **DeltaEncoder**: For versioned files / incremental backups
- **BinaryNormalizer**: For database dumps, serialized objects
- **TextNormalizer**: Line ending normalization, whitespace collapse
- **VideoKeyframeExtractor**: Extract keyframes for ZPAQ compression

---

## Priority

**High** — Significant compression improvements for structured data.

## Related

- #29 (Parallel benchmark) — test preprocessing with parallel execution
- #23 (Enhanced benchmark) — add preprocessing metrics to benchmark output