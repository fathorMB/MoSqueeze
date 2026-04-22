# Worker Spec: Add Preprocessing Support to Benchmark Tool (Issue #43)

## Overview

Add `--preprocess` flag to `mosqueeze-bench` to enable preprocessing during benchmarks, allowing comparison of compression with and without file-specific optimizations.

## Background

MoSqueeze already has a preprocessing system that can improve compression ratios for specific file types:

| Preprocessor | File Types | Estimated Improvement |
|--------------|------------|----------------------|
| `bayer-raw` | RAW images (RAF, DNG, NEF, CR2) | ~8% |
| `image-meta-strip` | JPEG, PNG, RAW | 2-5% |
| `json-canonical` | JSON files | 5-15% |
| `xml-canonical` | XML files | 3-10% |
| `zstd-dict` | Any (trained dictionary) | 5-20% |

Currently, `mosqueeze-bench` does NOT use any preprocessing. This makes it impossible to measure real-world compression potential for files that benefit from it.

### Real-World Motivation

Fujifilm RAF files (Lossless Compressed RAW) show only 1-3% compression. With `BayerPreprocessor`, this could improve to ~10% because:
1. RAF contains X-Trans sensor data with predictable patterns
2. Metadata can be separated and compressed differently
3. Pixel data reorganization improves entropy coding efficiency

---

## Implementation

### Part 1: Extend BenchmarkConfig

**File:** `src/libmosqueeze/include/mosqueeze/bench/BenchmarkConfig.hpp`

Add preprocessing configuration:

```cpp
struct BenchmarkConfig {
    // ... existing fields ...
    
    // Preprocessing options
    std::string preprocessMode = "none";  // "none", "auto", "bayer-raw", "image-meta-strip", etc.
    
    bool usePreprocessing() const { return preprocessMode != "none"; }
    bool autoPreprocess() const { return preprocessMode == "auto"; }
};
```

### Part 2: Extend BenchmarkResult

**File:** `src/libmosqueeze/include/mosqueeze/bench/BenchmarkResult.hpp`

Add preprocessing metrics:

```cpp
struct PreprocessMetrics {
    std::string type = "none";        // Preprocessor name
    size_t originalBytes = 0;          // Bytes before preprocessing
    size_t processedBytes = 0;          // Bytes after preprocessing
    double preprocessingTimeMs = 0.0;  // Time spent preprocessing
    double improvement = 0.0;          // Estimated improvement ratio
    bool applied = false;              // Whether preprocessing was actually used
};

struct BenchmarkResult {
    std::string algorithm;
    int level = 0;
    std::filesystem::path file;
    FileType fileType = FileType::Unknown;
    size_t originalBytes = 0;
    size_t compressedBytes = 0;
    std::chrono::milliseconds encodeTime{0};
    std::chrono::milliseconds decodeTime{0};
    size_t peakMemoryBytes = 0;
    PreprocessMetrics preprocess;  // NEW

    double ratio() const {
        return compressedBytes > 0
            ? static_cast<double>(originalBytes) / compressedBytes
            : 0.0;
    }
    
    // Total compression ratio including preprocessing
    double totalRatio() const {
        if (!preprocess.applied || preprocess.processedBytes == 0) {
            return ratio();
        }
        return preprocess.originalBytes > 0
            ? static_cast<double>(preprocess.originalBytes) / compressedBytes
            : 0.0;
    }
};
```

### Part 3: Update BenchmarkRunner

**File:** `src/mosqueeze-bench/src/BenchmarkRunner.cpp`

Add preprocessing support to `runIteration`:

```cpp
#include <mosqueeze/PreprocessorSelector.hpp>
#include <mosqueeze/preprocessors/BayerPreprocessor.hpp>
#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>
#include <mosqueeze/CompressionPipeline.hpp>

namespace {

std::unique_ptr<IPreprocessor> createPreprocessor(const std::string& name) {
    if (name == "bayer-raw") {
        return std::make_unique<BayerPreprocessor>();
    }
    if (name == "image-meta-strip") {
        return std::make_unique<ImageMetaStripper>();
    }
    if (name == "json-canonical") {
        return std::make_unique<JsonCanonicalizer>();
    }
    if (name == "xml-canonical") {
        return std::make_unique<XmlCanonicalizer>();
    }
    return nullptr;
}

} // namespace

BenchmarkResult BenchmarkRunner::runIteration(
    ICompressionEngine* engine,
    const std::filesystem::path& file,
    int level,
    const BenchmarkConfig& config,
    FileType fileType) const {
    
    // Read file into memory
    std::ifstream inFile(file, std::ios::binary);
    if (!inFile) {
        throw std::runtime_error("Failed to open file: " + file.string());
    }
    std::ostringstream fileContent;
    fileContent << inFile.rdbuf();
    std::string rawContent = fileContent.str();
    
    // Apply preprocessing if enabled
    std::string contentToCompress = rawContent;
    PreprocessMetrics preprocessMetrics;
    
    if (config.usePreprocessing()) {
        std::unique_ptr<IPreprocessor> preprocessor;
        
        if (config.autoPreprocess()) {
            PreprocessorSelector selector;
            preprocessor = std::unique_ptr<IPreprocessor>(selector.selectBest(fileType));
        } else {
            preprocessor = createPreprocessor(config.preprocessMode);
        }
        
        if (preprocessor && preprocessor->canProcess(fileType)) {
            std::istringstream input(rawContent);
            std::ostringstream output;
            
            auto preprocessStart = std::chrono::steady_clock::now();
            PreprocessResult result = preprocessor->preprocess(input, output, fileType);
            auto preprocessEnd = std::chrono::steady_clock::now();
            
            contentToCompress = output.str();
            preprocessMetrics.type = preprocessor->name();
            preprocessMetrics.originalBytes = result.originalBytes;
            preprocessMetrics.processedBytes = result.processedBytes;
            preprocessMetrics.preprocessingTimeMs = std::chrono::duration<double, std::milli>(
                preprocessEnd - preprocessStart).count();
            preprocessMetrics.improvement = preprocessor->estimatedImprovement(fileType);
            preprocessMetrics.applied = true;
        } else {
            preprocessMetrics.type = config.preprocessMode;
            preprocessMetrics.applied = false;
            preprocessMetrics.originalBytes = rawContent.size();
            preprocessMetrics.processedBytes = rawContent.size();
        }
    }
    
    // Compress
    std::istringstream input(contentToCompress);
    std::ostringstream compressed;
    
    CompressionOptions opts{};
    opts.level = level;
    opts.extreme = level >= engine->maxLevel();
    
    auto encodeStart = std::chrono::steady_clock::now();
    CompressionResult encodeResult = engine->compress(input, compressed, opts);
    auto encodeEnd = std::chrono::steady_clock::now();
    auto encodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(encodeEnd - encodeStart);
    
    // Decompress if requested
    std::chrono::milliseconds decodeDuration{0};
    if (config.runDecode) {
        std::istringstream compressedInput(compressed.str());
        std::ostringstream decompressed;
        
        // Note: For full pipeline, need to postprocess after decompress
        // This is a simplified version - full implementation needs to 
        // store preprocessing metadata and apply postprocess()
        
        auto decodeStart = std::chrono::steady_clock::now();
        engine->decompress(compressedInput, decompressed);
        auto decodeEnd = std::chrono::steady_clock::now();
        decodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(decodeEnd - decodeStart);
    }
    
    // Build result
    BenchmarkResult row{};
    row.algorithm = engine->name();
    row.level = level;
    row.file = file;
    row.fileType = fileType;
    row.originalBytes = config.usePreprocessing() 
        ? preprocessMetrics.originalBytes 
        : encodeResult.originalBytes;
    row.compressedBytes = encodeResult.compressedBytes;
    row.encodeTime = encodeDuration;
    row.decodeTime = decodeDuration;
    row.peakMemoryBytes = config.trackMemory ? encodeResult.peakMemoryBytes : 0;
    row.preprocess = preprocessMetrics;
    
    return row;
}
```

### Part 4: Update CLI

**File:** `src/mosqueeze-bench/src/main.cpp`

Add CLI option:

```cpp
std::string preprocessMode = "none";

app.add_option("--preprocess", preprocessMode, 
    "Preprocessor: auto, none, bayer-raw, image-meta-strip, json-canonical, xml-canonical")
    ->check(CLI::IsMember({"auto", "none", "bayer-raw", "image-meta-strip", "json-canonical", "xml-canonical"}))
    ->default_val("none");

// In dry-run output:
if (dryRun) {
    std::cout << "  preprocess: " << config.preprocessMode << '\n';
    // ... rest of config
}

config.preprocessMode = preprocessMode;
```

### Part 5: Update ResultsStore

**File:** `src/mosqueeze-bench/src/ResultsStore.cpp`

Add preprocessing columns to CSV/JSON export:

```cpp
// In initDatabase():
CREATE TABLE benchmark_results (
    // ... existing columns ...
    preprocess_type TEXT DEFAULT 'none',
    preprocess_original_bytes INTEGER DEFAULT 0,
    preprocess_processed_bytes INTEGER DEFAULT 0,
    preprocess_time_ms REAL DEFAULT 0,
    preprocess_applied INTEGER DEFAULT 0
);

// In save():
sqlite3_bind_text(stmt, 10, result.preprocess.type.c_str(), -1, SQLITE_TRANSIENT);
sqlite3_bind_int64(stmt, 11, result.preprocess.originalBytes);
sqlite3_bind_int64(stmt, 12, result.preprocess.processedBytes);
sqlite3_bind_double(stmt, 13, result.preprocess.preprocessingTimeMs);
sqlite3_bind_int(stmt, 14, result.preprocess.applied ? 1 : 0);

// In exportCsv():
out << ",preprocess_type,preprocess_original,preprocess_processed,preprocess_time_ms\n";

// In exportJson():
payload.push_back({
    {"preprocess", {
        {"type", row.preprocess.type},
        {"originalBytes", row.preprocess.originalBytes},
        {"processedBytes", row.preprocess.processedBytes},
        {"timeMs", row.preprocess.preprocessingTimeMs},
        {"applied", row.preprocess.applied}
    }}
});
```

### Part 6: Update Formatters

**File:** `src/mosqueeze-bench/src/Formatters.cpp`

Update pretty-print and markdown output:

```cpp
std::string formatVerbose(const BenchmarkResult& result, const BenchmarkStats* stats) {
    std::ostringstream out;
    
    // ... existing output ...
    
    if (result.preprocess.applied) {
        out << "  Preprocess: " << result.preprocess.type << "\n";
        out << "    Original: " << formatBytes(result.preprocess.originalBytes) << "\n";
        out << "    Processed: " << formatBytes(result.preprocess.processedBytes) << "\n";
        out << "    Time: " << result.preprocess.preprocessingTimeMs << "ms\n";
        out << "    Improvement: " << (result.preprocess.improvement * 100) << "%\n";
    }
    
    return out.str();
}
```

---

## Testing

### Unit Tests

**File:** `tests/unit/BenchmarkPreprocess_test.cpp`

```cpp
#include <mosqueeze/bench/BenchmarkRunner.hpp>
#include <mosqueeze/bench/BenchmarkConfig.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <catch2/catch.hpp>

TEST_CASE("BenchmarkConfig preprocessing", "[benchmark]") {
    SECTION("default is none") {
        BenchmarkConfig config;
        REQUIRE(config.preprocessMode == "none");
        REQUIRE_FALSE(config.usePreprocessing());
    }
    
    SECTION("auto mode enabled") {
        BenchmarkConfig config;
        config.preprocessMode = "auto";
        REQUIRE(config.usePreprocessing());
        REQUIRE(config.autoPreprocess());
    }
    
    SECTION("specific preprocessor") {
        BenchmarkConfig config;
        config.preprocessMode = "bayer-raw";
        REQUIRE(config.usePreprocessing());
        REQUIRE_FALSE(config.autoPreprocess());
    }
}

TEST_CASE("Benchmark with preprocessing", "[benchmark][preprocess]") {
    SECTION("bayer-raw on RAW file") {
        BenchmarkConfig config;
        config.files = {testDataPath("sample.RAF")};
        config.algorithms = {"zstd"};
        config.defaultOnly = true;
        config.preprocessMode = "bayer-raw";
        
        BenchmarkRunner runner;
        runner.registerEngine(std::make_unique<ZstdEngine>());
        
        auto results = runner.runWithConfig(config);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].preprocess.applied);
        REQUIRE(results[0].preprocess.type == "bayer-raw");
    }
    
    SECTION("none mode - no preprocessing") {
        BenchmarkConfig config;
        config.files = {testDataPath("sample.RAF")};
        config.preprocessMode = "none";
        
        // Run with and without preprocessing, compare
        BenchmarkRunner runner;
        runner.registerEngine(std::make_unique<ZstdEngine>());
        
        auto resultsNone = runner.runWithConfig(config);
        config.preprocessMode = "bayer-raw";
        auto resultsPreprocessed = runner.runWithConfig(config);
        
        // Preprocessed should have better or equal compression
        REQUIRE(resultsPreprocessed[0].preprocess.applied);
    }
}
```

### Integration Test

```cpp
TEST_CASE("Full benchmark with preprocessing", "[integration]") {
    // Create temporary directory
    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "mosqueeze_test";
    std::filesystem::create_directories(tempDir);
    
    // Run benchmark
    int result = std::system(
        "mosqueeze-bench "
        "-d test_data/raw_images "
        "--preprocess auto "
        "--default-only "
        "-o " + tempDir.string()
    );
    REQUIRE(result == 0);
    
    // Check results.json contains preprocess info
    std::ifstream results(tempDir / "results.json");
    nlohmann::json json;
    results >> json;
    
    for (const auto& row : json) {
        REQUIRE(row.contains("preprocess"));
        // For RAW files, preprocess.type should be "bayer-raw" or "image-meta-strip"
        if (row["file"].get<std::string>().ends_with(".RAF") ||
            row["file"].get<std::string>().ends_with(".DNG")) {
            REQUIRE(row["preprocess"]["applied"].get<bool>());
        }
    }
}
```

---

## Example Usage

```bash
# Benchmark RAW files with BayerPreprocessor
mosqueeze-bench -d ./raw_images --preprocess bayer-raw --default-only -v

# Benchmark JSON files with json-canonical preprocessor
mosqueeze-bench -d ./json_data --preprocess json-canonical -a zstd -v

# Auto-select best preprocessor per file type
mosqueeze-bench -d ./mixed_data --preprocess auto --all-engines

# Compare with and without preprocessing
mosqueeze-bench -d ./raw_images --preprocess none -o results_none.json
mosqueeze-bench -d ./raw_images --preprocess bayer-raw -o results_bayer.json
mosqueeze-viz --compare results_none.json,results_bayer.json -o comparison.html
```

---

## Example Output

```json
{
  "algorithm": "zstd",
  "level": 22,
  "file": "G:\\photos\\_DSF6950.RAF",
  "fileType": 4,
  "originalBytes": 25914032,
  "compressedBytes": 23500000,
  "encodeMs": 28500,
  "decodeMs": 230,
  "ratio": 1.103,
  "preprocess": {
    "type": "bayer-raw",
    "originalBytes": 25914032,
    "processedBytes": 25500000,
    "timeMs": 45.2,
    "improvement": 0.08,
    "applied": true
  }
}
```

---

## Success Criteria

- [ ] `--preprocess` flag works with all valid preprocessor names
- [ ] `--preprocess auto` selects appropriate preprocessor per file type
- [ ] Benchmark results include preprocessing metrics in JSON output
- [ ] CSV export includes preprocessing columns
- [ ] Backward compatible: default is `--preprocess none` (current behavior)
- [ ] Preprocessing time is measured and reported separately from encode time
- [ ] Unit tests pass for preprocessing logic
- [ ] Integration test confirms RAW files show `preprocess.applied: true`

---

## Estimated Effort

| Component | Lines of Code | Hours |
|-----------|---------------|-------|
| BenchmarkConfig extension | ~20 | 1 |
| BenchmarkResult extension | ~30 | 1 |
| BenchmarkRunner integration | ~100 | 4 |
| CLI option | ~15 | 0.5 |
| ResultsStore update | ~50 | 2 |
| Formatters update | ~30 | 1 |
| Unit tests | ~150 | 3 |
| Integration tests | ~50 | 2 |
| **Total** | ~445 | **14.5** |

**Estimated: 2-3 days**

---

## Related

- Issue #43 - GitHub issue
- `BayerPreprocessor` - `src/libmosqueeze/src/preprocessors/BayerPreprocessor.cpp`
- `CompressionPipeline` - `src/libmosqueeze/src/CompressionPipeline.cpp`
- `PreprocessorSelector` - `src/libmosqueeze/include/mosqueeze/PreprocessorSelector.hpp`