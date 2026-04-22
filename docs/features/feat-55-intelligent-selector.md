# Worker Spec: Intelligent Algorithm Selection Engine

**Issue:** #55 - Feature: Intelligent Algorithm Selection Engine  
**Branch:** `feat/intelligent-selector`  
**Priority:** High (Core Feature)  
**Estimated Effort:** 3-4 weeks  

---

## Vision

Transform MoSqueeze from a manual compression tool into an **intelligent compression engine** that knows how to compress any file optimally.

> "Give me any file, and I'll tell you how to compress it best."

**Primary Goal:** Best compression ratio (MinSize) - always optimize for smallest output.

---

## User Experience

### Before (Manual)

```bash
# User must know:
# - What preprocessor to use
# - What algorithm to use  
# - What compression level
mosqueeze-bench -d ./files/ -a zstd --preprocess bayer-raw
```

### After (Intelligent)

```bash
# Library decides everything
mosqueeze-compress ./photo.raf

# Output:
#   File: photo.raf (27.3 MB)
#   Detected: Fujifilm RAF (Lossless Compressed RAW)
#   Entropy: 7.2 bits/byte (high, already compressed)
#   
#   ╔═══════════════════════════════════════════════════════════╗
#   ║  BEST OPTION (MinSize)                                    ║
#   ╠═══════════════════════════════════════════════════════════╣
#   ║  Preprocessor: none                                       ║
#   ║  Algorithm:     zstd                                      ║
#   ║  Level:         19                                        ║
#   ║  Expected ratio: 0.88 (12% reduction)                    ║
#   ║  Estimated time: 2.1s                                     ║
#   ║  Confidence:    94% ████████████████████░░░              ║
#   ╚═══════════════════════════════════════════════════════════╝
#   
#   Alternatives:
#     • Fastest:    zstd level 3  → ratio 0.91, time 0.4s
#     • Balanced:   zstd level 9  → ratio 0.89, time 1.2s
#   
#   Proceed? [Y/n/customize/explain]
```

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Intelligent Selection Engine                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌─────────────────────────────────────────────────────────────┐  │
│   │                     Input Processing                         │  │
│   │  • File path or memory buffer                                │  │
│   │  • Optional: user hints (algorithm preference, time budget)  │  │
│   └─────────────────────────────────────────────────────────────┘  │
│                              │                                       │
│                              ▼                                       │
│   ┌─────────────────────────────────────────────────────────────┐  │
│   │                     File Analyzer                            │  │
│   │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │  │
│   │  │ Magic Bytes │  │ Entropy     │  │ Pattern     │         │  │
│   │  │ Detection   │  │ Calculation │  │ Analysis    │         │  │
│   │  └─────────────┘  └─────────────┘  └─────────────┘         │  │
│   │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │  │
│   │  │ Structure   │  │ Metadata    │  │ Chunk       │         │  │
│   │  │ Detection   │  │ Extraction  │  │ Analysis    │         │  │
│   │  └─────────────┘  └─────────────┘  └─────────────┘         │  │
│   └─────────────────────────────────────────────────────────────┘  │
│                              │                                       │
│                              ▼                                       │
│   ┌─────────────────────────────────────────────────────────────┐  │
│   │                    File Features                             │  │
│   │  {                                                           │  │
│   │    detectedType: "image/x-fuji-raf",                        │  │
│   │    extension: ".raf",                                        │  │
│   │    fileSize: 28608512,                                       │  │
│   │    entropy: 7.21,                                            │  │
│   │    entropyDistribution: [7.0, 7.2, 7.3, ...],              │  │
│   │    repeatPatterns: 142,                                      │  │
│   │    chunkRatio: 0.85,                                        │  │
│   │    isStructured: false,                                     │  │
│   │    hasMetadata: true,                                       │  │
│   │    rawFormat: {                                             │  │
│   │      compression: "LosslessCompressed",                    │  │
│   │      manufacturer: "Fujifilm"                               │  │
│   │    }                                                        │  │
│   │  }                                                           │  │
│   └─────────────────────────────────────────────────────────────┘  │
│                              │                                       │
│                              ▼                                       │
│   ┌─────────────────────────────────────────────────────────────┐  │
│   │                   Decision Engine                            │  │
│   │                                                              │  │
│   │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │  │
│   │  │ Database    │  │ Heuristics  │  │ Scorer      │         │  │
│   │  │ Lookup      │  │ Fallback    │  │ & Ranker    │         │  │
│   │  └─────────────┘  └─────────────┘  └─────────────┘         │  │
│   │         │                │                │                 │  │
│   │         └────────────────┼────────────────┘                 │  │
│   │                          │                                  │  │
│   │                          ▼                                  │  │
│   │              ┌───────────────────┐                          │  │
│   │              │ Recommendation    │                          │  │
│   │              │ • Preprocessor     │                          │  │
│   │              │ • Algorithm        │                          │  │
│   │              │ • Level            │                          │  │
│   │              │ • Confidence       │                          │  │
│   │              │ • Explanation      │                          │  │
│   │              └───────────────────┘                          │  │
│   └─────────────────────────────────────────────────────────────┘  │
│                              │                                       │
│                              ▼                                       │
│   ┌─────────────────────────────────────────────────────────────┐  │
│   │                   Benchmark Database                          │  │
│   │  ┌─────────────────────────────────────────────────────┐   │  │
│   │  │ SQLite (~20MB)                                       │   │  │
│   │  │ • 100,000+ compression results                      │   │  │
│   │  │ • 500+ file types                                    │   │  │
│   │  │ • All algorithm/level/preprocessor combos           │   │  │
│   │  │ • Precomputed for fast lookup                       │   │  │
│   │  └─────────────────────────────────────────────────────┘   │  │
│   │                                                              │  │
│   │  ┌─────────────────────────────────────────────────────┐   │  │
│   │  │ Online Learning (Optional)                           │   │  │
│   │  │ • Record actual compression results                 │   │  │
│   │  │ • Improve predictions over time                      │   │  │
│   │  │ • Per-user customization                            │   │  │
│   │  └─────────────────────────────────────────────────────┘   │  │
│   └─────────────────────────────────────────────────────────────┘  │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Component Design

### 1. File Analyzer

```cpp
// src/libmosqueeze/include/mosqueeze/FileAnalyzer.hpp
#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>
#include <optional>

namespace mosqueeze {

struct FileFeatures {
    // Basic identification
    std::string detectedType;        // "image/png", "application/json"
    std::string extension;           // ".png", ".json"
    std::vector<uint8_t> magicBytes;  // First 16 bytes
    
    // Size metrics
    size_t fileSize;
    size_t headerSize;               // Detected header/metadata size
    size_t payloadSize;              // Actual content size
    
    // Entropy metrics
    double entropy;                  // Shannon entropy (bits/byte)
    std::vector<double> entropyBuckets;  // Entropy per chunk
    double entropyVariance;          // How uniform is entropy
    
    // Pattern metrics
    size_t repeatPatterns;           // Count of repeated sequences > 4 bytes
    double chunkRatio;               // LZ77 compression potential
    size_t uniqueBytes;              // Unique byte count
    
    // Structure detection
    bool isStructured;               // JSON/XML parseable
    std::string structureType;        // "json", "xml", "none"
    bool hasMetadata;                // Image/video metadata present
    
    // Type-specific
    std::optional<struct {
        std::string compression;      // "uncompressed", "lossless", "lossy"
        std::string manufacturer;     // "Fujifilm", "Sony", etc.
    }> rawFormat;
    
    std::optional<struct {
        int width;
        int height;
        int bitDepth;
        bool hasAlpha;
        bool interlaced;
    }> imageInfo;
};

class FileAnalyzer {
public:
    FileAnalyzer();
    ~FileAnalyzer();
    
    FileFeatures analyze(const std::filesystem::path& file);
    FileFeatures analyze(std::span<const uint8_t> data, 
                         std::string_view filenameHint = "");
    
private:
    // Detection helpers
    std::string detectMimeType(std::span<const uint8_t> data);
    double calculateEntropy(std::span<const uint8_t> data);
    std::vector<double> calculateEntropyBuckets(std::span<const uint8_t> data, 
                                                 size_t bucketCount = 16);
    size_t countRepeatPatterns(std::span<const uint8_t> data);
    double calculateChunkRatio(std::span<const uint8_t> data);
    bool detectStructure(std::span<const uint8_t> data, std::string& type);
    
    // Type-specific analyzers
    void analyzeRawFormat(std::span<const uint8_t> data, FileFeatures& features);
    void analyzeImageInfo(std::span<const uint8_t> data, FileFeatures& features);
};

} // namespace mosqueeze
```

### 2. Benchmark Database

```cpp
// src/libmosqueeze/include/mosqueeze/BenchmarkDatabase.hpp
#pragma once

#include <filesystem>
#include <string>
#include <optional>
#include <vector>
#include <chrono>

namespace mosqueeze {

struct BenchmarkEntry {
    // File identification
    std::string fileHash;            // SHA256 of file content
    std::string fileType;            // Detected MIME type
    std::string extension;
    size_t fileSize;
    
    // File features (for similarity matching)
    double entropy;
    double chunkRatio;
    size_t repeatPatterns;
    bool isStructured;
    
    // Compression parameters
    std::string preprocessor;        // "none", "bayer-raw", etc.
    std::string algorithm;           // "zstd", "brotli", etc.
    int level;
    
    // Results
    size_t compressedSize;
    double ratio;                    // compressedSize / fileSize
    std::chrono::milliseconds compressionTime;
    std::chrono::milliseconds decompressionTime;
    size_t peakMemory;
};

struct QueryResult {
    std::string preprocessor;
    std::string algorithm;
    int level;
    double avgRatio;
    double avgTimeMs;
    size_t sampleCount;
    double confidence;
};

class BenchmarkDatabase {
public:
    BenchmarkDatabase();
    ~BenchmarkDatabase();
    
    // Database management
    void open(const std::filesystem::path& dbPath);
    void close();
    bool isOpen() const;
    
    // Query operations
    std::vector<QueryResult> queryBestForType(
        const std::string& fileType,
        size_t maxResults = 10
    );
    
    std::vector<QueryResult> querySimilar(
        const FileFeatures& features,
        size_t maxResults = 10
    );
    
    std::optional<QueryResult> queryBestOverall(
        const std::string& fileType,
        const std::string& algorithm = ""
    );
    
    // Insert operations
    void insertResult(const BenchmarkEntry& entry);
    void insertResults(const std::vector<BenchmarkEntry>& entries);
    
    // Statistics
    size_t totalEntries() const;
    size_t entriesForType(const std::string& fileType) const;
    std::vector<std::string> supportedFileTypes() const;
    
    // Online learning
    void enableOnlineLearning(const std::filesystem::path& userDbPath);
    void disableOnlineLearning();
    bool isOnlineLearningEnabled() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mosqueeze
```

### 3. Decision Engine

```cpp
// src/libmosqueeze/include/mosqueeze/IntelligentSelector.hpp
#pragma once

#include "FileAnalyzer.hpp"
#include "BenchmarkDatabase.hpp"
#include <filesystem>
#include <vector>
#include <optional>

namespace mosqueeze {

enum class OptimizationGoal {
    MinSize,          // Best compression ratio (DEFAULT)
    Fastest,          // Fastest compression
    Balanced,         // Trade-off size/time
    MinMemory,        // Lowest memory usage
    BestDecompression // Fastest decompression
};

struct Recommendation {
    // Primary recommendation
    std::string preprocessor;        // "none", "bayer-raw", etc.
    std::string algorithm;           // "zstd", "brotli", etc.
    int level;
    
    // Expected results
    double expectedRatio;
    double expectedTimeMs;
    size_t expectedSize;
    
    // Confidence
    double confidence;               // 0.0 - 1.0
    size_t sampleCount;              // How many similar files tested
    
    // Explanation
    std::string explanation;         // Human-readable explanation
    
    // Alternatives for different goals
    std::vector<Recommendation> alternatives;
};

class IntelligentSelector {
public:
    explicit IntelligentSelector(OptimizationGoal goal = OptimizationGoal::MinSize);
    ~IntelligentSelector();
    
    // Main API
    Recommendation analyze(const std::filesystem::path& file);
    Recommendation analyze(std::span<const uint8_t> data, 
                          std::string_view filenameHint = "");
    
    // Interactive mode (prints progress and asks confirmation)
    Recommendation analyzeInteractive(const std::filesystem::path& file);
    
    // With alternative suggestions
    Recommendation analyzeWithAlternatives(
        const std::filesystem::path& file,
        std::vector<OptimizationGoal> alternativeGoals = {
            OptimizationGoal::Fastest,
            OptimizationGoal::Balanced
        }
    );
    
    // Learning
    void recordResult(const std::filesystem::path& file,
                     const std::string& preprocessor,
                     const std::string& algorithm,
                     int level,
                     size_t compressedSize,
                     std::chrono::milliseconds compressionTime);
    
    // Database management
    void loadBenchmarkDatabase(const std::filesystem::path& dbPath);
    void setOnlineLearning(bool enabled, 
                          const std::filesystem::path& userDbPath = "");
    
    // Configuration
    void setOptimizationGoal(OptimizationGoal goal);
    void setMaxAlternatives(size_t count);
    void setConfidenceThreshold(double threshold);
    
private:
    // Decision logic
    Recommendation queryDatabase(const FileFeatures& features);
    Recommendation heuristicFallback(const FileFeatures& features);
    Recommendation scoreAndRank(const std::vector<QueryResult>& results,
                               const FileFeatures& features);
    
    // Explanation generation
    std::string generateExplanation(const FileFeatures& features,
                                   const Recommendation& rec);
    
    // Confidence calculation
    double calculateConfidence(const QueryResult& result,
                             const FileFeatures& features);
    
    FileAnalyzer analyzer_;
    BenchmarkDatabase database_;
    OptimizationGoal goal_;
    size_t maxAlternatives_ = 3;
    double confidenceThreshold_ = 0.7;
};

} // namespace mosqueeze
```

---

## Database Schema

```sql
-- File metadata
CREATE TABLE files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_hash TEXT UNIQUE NOT NULL,
    file_type TEXT,
    extension TEXT,
    file_size INTEGER,
    
    -- Features
    entropy REAL,
    chunk_ratio REAL,
    repeat_patterns INTEGER,
    is_structured INTEGER,
    has_metadata INTEGER,
    
    -- Timestamps
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_files_type ON files(file_type);
CREATE INDEX idx_files_hash ON files(file_hash);
CREATE INDEX idx_files_entropy ON files(entropy);

-- Compression results
CREATE TABLE results (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    
    -- Compression parameters
    preprocessor TEXT NOT NULL DEFAULT 'none',
    algorithm TEXT NOT NULL,
    compression_level INTEGER NOT NULL,
    
    -- Results
    compressed_size INTEGER NOT NULL,
    ratio REAL NOT NULL,
    compression_time_ms INTEGER NOT NULL,
    decompression_time_ms INTEGER,
    peak_memory_kb INTEGER,
    
    -- Metadata
    notes TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (file_id) REFERENCES files(id)
);

CREATE INDEX idx_results_file ON results(file_id);
CREATE INDEX idx_results_algo ON results(algorithm);
CREATE INDEX idx_results_preproc ON results(preprocessor);
CREATE INDEX idx_results_ratio ON results(ratio);

-- Best results view (one row per file)
CREATE VIEW best_results AS
SELECT 
    f.file_type,
    f.extension,
    f.file_size,
    f.entropy,
    r.preprocessor,
    r.algorithm,
    r.compression_level,
    r.ratio,
    r.compression_time_ms,
    COUNT(*) as sample_count
FROM files f
JOIN results r ON f.id = r.file_id
GROUP BY f.file_type, f.extension
HAVING r.ratio = MIN(r.ratio);

-- File type statistics
CREATE VIEW type_stats AS
SELECT 
    file_type,
    extension,
    COUNT(DISTINCT file_hash) as file_count,
    AVG(entropy) as avg_entropy,
    AVG(file_size) as avg_size,
    MIN(file_size) as min_size,
    MAX(file_size) as max_size
FROM files
GROUP BY file_type, extension;

-- Algorithm performance per type
CREATE VIEW algo_performance AS
SELECT 
    f.file_type,
    r.algorithm,
    r.compression_level,
    AVG(r.ratio) as avg_ratio,
    AVG(r.compression_time_ms) as avg_time_ms,
    COUNT(*) as sample_count
FROM files f
JOIN results r ON f.id = f.file_id
GROUP BY f.file_type, r.algorithm, r.compression_level
ORDER BY f.file_type, avg_ratio;
```

---

## CLI Commands

### mosqueeze-suggest

```bash
# Analyze single file
mosqueeze-suggest ./photo.raf

# Analyze directory
mosqueeze-suggest ./files/ --recursive

# Output format
mosqueeze-suggest ./file.png --format json
mosqueeze-suggest ./file.png --format markdown
mosqueeze-suggest ./file.png --format detailed

# With constraints
mosqueeze-suggest ./file.json --max-time 5s
mosqueeze-suggest ./file.json --algorithm zstd
mosqueeze-suggest ./file.json --goal fastest
```

### mosqueeze-analyze

```bash
# Directory analysis
mosqueeze-analyze ./files/

# Output:
# Directory: ./files/ (47 files, 1.2 GB)
# 
# File type distribution:
#   PNG:    23 files (500 MB, 42%)
#   JSON:   12 files ( 50 MB,  4%)
#   RAF:     8 files (400 MB, 33%)
#   Other:   4 files (250 MB, 21%)
# 
# Recommended compression strategy:
#   PNG files:
#     → PngOptimizer + zstd level 19
#     → Expected savings: 120 MB (24%)
#   
#   JSON files:
#     → JsonCanonicalizer + brotli level 11
#     → Expected savings: 35 MB (70%)
#   
#   RAF files:
#     → zstd level 19 (no preprocessor)
#     → Expected savings: 48 MB (12%)
# 
# Total expected savings: 203 MB (17%)
# Estimated time: 8.5 minutes
# 
# Export report? [Y/n/format]
```

### mosqueeze-compress

```bash
# Intelligent compression
mosqueeze-compress ./file.raf -o ./compressed/

# Interactive mode
mosqueeze-compress ./file.raf --interactive
# Shows recommendation, asks confirmation

# Batch with learning
mosqueeze-compress ./files/ --learn
# Records results, improves future predictions

# Skip confirmation
mosqueeze-compress ./files/ --yes
```

---

## Heuristic Fallback Rules

When database has no data for a file type, use heuristics:

```cpp
Recommendation heuristicFallback(const FileFeatures& features) {
    // JSON files
    if (features.detectedType == "application/json") {
        return {
            .preprocessor = "json-canon",
            .algorithm = "brotli",
            .level = 11,
            .expectedRatio = 0.15,  // 85% reduction for JSON
            .confidence = 0.85
        };
    }
    
    // XML files
    if (features.detectedType == "application/xml" || 
        features.detectedType == "text/xml") {
        return {
            .preprocessor = "xml-canon",
            .algorithm = "brotli",
            .level = 11,
            .expectedRatio = 0.20,
            .confidence = 0.80
        };
    }
    
    // PNG files
    if (features.detectedType == "image/png") {
        return {
            .preprocessor = "png-optimizer",
            .algorithm = "zstd",
            .level = 19,
            .expectedRatio = 0.75,  // 25% reduction for PNG
            .confidence = 0.75
        };
    }
    
    // RAW files (check compression type)
    if (features.rawFormat) {
        if (features.rawFormat->compression == "Uncompressed") {
            return {
                .preprocessor = "bayer-raw",
                .algorithm = "zstd",
                .level = 19,
                .expectedRatio = 0.80,  // 20% reduction
                .confidence = 0.70
            };
        } else {
            return {
                .preprocessor = "none",  // Already compressed
                .algorithm = "zstd",
                .level = 19,
                .expectedRatio = 0.88,  // 12% reduction
                .confidence = 0.90
            };
        }
    }
    
    // High entropy files (already compressed)
    if (features.entropy > 7.5) {
        return {
            .preprocessor = "none",
            .algorithm = "zstd",
            .level = 19,
            .expectedRatio = 0.95,  // 5% reduction at best
            .confidence = 0.70,
            .explanation = "High entropy suggests already-compressed data; "
                          "compression gains will be minimal"
        };
    }
    
    // Structured data with low entropy
    if (features.isStructured && features.entropy < 6.0) {
        return {
            .preprocessor = "none",
            .algorithm = "brotli",
            .level = 11,
            .expectedRatio = 0.10,  // 90% reduction possible
            .confidence = 0.65
        };
    }
    
    // Default: zstd level 19
    return {
        .preprocessor = "none",
        .algorithm = "zstd",
        .level = 19,
        .expectedRatio = 0.85,
        .confidence = 0.50,
        .explanation = "Unknown file type; using universal best-effort compression"
    };
}
```

---

## Implementation Phases

### Phase 0: Benchmark Corpus (Prerequisite)

**Duration:** 1-2 weeks  
**Goal:** Create diverse file corpus and run all combinations

**Tasks:**
1. [ ] Collect benchmark corpus (~500 files)
   - images/: PNG, JPEG, WebP, RAW (RAF, ARW, NEF, CR2)
   - documents/: JSON, XML, HTML, CSV, Markdown
   - source-code/: JS, Python, C++, minified versions
   - archives/: TAR, ZIP
   - binary/: EXE, DLL, databases

2. [ ] Run extended benchmarks
   - All preprocessors × All algorithms × Multiple levels
   - ~100,000 combinations
   - Measure: size, time, memory

3. [ ] Build SQLite database
   - Import results
   - Create indexes
   - Verify query performance (< 100ms)

4. [ ] Ship database with library
   - Embed as resource or separate download
   - ~20-50MB size

### Phase 1: File Analyzer (3-4 days)

**Duration:** 3-4 days  
**Goal:** Extract features from files

**Tasks:**
1. [ ] Implement `FileAnalyzer` class
   - Magic bytes detection
   - Entropy calculation
   - Pattern analysis
   - Structure detection

2. [ ] Type-specific analyzers
   - RAW format detection
   - Image info extraction
   - JSON/XML parsing check

3. [ ] Unit tests
   - Test each file type
   - Edge cases (empty, tiny, huge)
   - Performance benchmarks

### Phase 2: Benchmark Database (2-3 days)

**Duration:** 2-3 days  
**Goal:** SQLite integration and query API

**Tasks:**
1. [ ] Implement `BenchmarkDatabase` class
   - Open/close connection
   - Query functions
   - Insert functions

2. [ ] Query optimization
   - Index creation
   - Prepared statements
   - Connection pooling

3. [ ] Unit tests
   - Query accuracy
   - Performance (< 100ms)
   - Online learning operations

### Phase 3: Decision Engine (4-5 days)

**Duration:** 4-5 days  
**Goal:** Generate recommendations

**Tasks:**
1. [ ] Implement `IntelligentSelector` class
   - Database lookup
   - Heuristic fallback
   - Scoring and ranking

2. [ ] Recommendation generation
   - Primary recommendation
   - Alternatives calculation
   - Confidence scoring

3. [ ] Explanation generation
   - Human-readable explanations
   - Why this algorithm/preprocessor
   - What was detected about the file

4. [ ] Unit tests
   - Known file types
   - Unknown file types
   - Confidence thresholds

### Phase 4: CLI Integration (2-3 days)

**Duration:** 2-3 days  
**Goal:** Commands for users

**Tasks:**
1. [ ] `mosqueeze-suggest` command
   - Single file analysis
   - Directory analysis
   - Output formats (text, JSON, markdown)

2. [ ] `mosqueeze-analyze` command
   - Batch analysis
   - Statistics generation
   - Report export

3. [ ] `mosqueeze-compress` integration
   - Interactive mode
   - --learn flag
   - --yes flag

### Phase 5: Documentation (1 day)

**Duration:** 1 day  
**Goal:** Complete documentation

**Tasks:**
1. [ ] API documentation
   - All classes and methods
   - Examples

2. [ ] CLI documentation
   - All commands
   - All flags

3. [ ] Architecture documentation
   - How it works
   - Database schema
   - Extending heuristics

### Phase 6: ML Model (Optional, Future)

**Duration:** 2-3 weeks  
**Goal:** Machine learning predictions

**Tasks:**
1. [ ] Feature engineering
2. [ ] Model training (LightGBM/XGBoost)
3. [ ] Model integration
4. [ ] A/B testing vs heuristics

---

## Test Cases

```cpp
// tests/intelligent_selector_test.cpp

TEST_CASE("FileAnalyzer: Detect PNG") {
    FileAnalyzer analyzer;
    auto features = analyzer.analyze(loadFile("test.png"));
    
    REQUIRE(features.detectedType == "image/png");
    REQUIRE(features.extension == ".png");
    REQUIRE(features.hasMetadata == true);
    REQUIRE(features.entropy > 6.0);
    REQUIRE(features.entropy < 8.0);
}

TEST_CASE("FileAnalyzer: Detect Fujifilm RAF") {
    FileAnalyzer analyzer;
    auto features = analyzer.analyze(loadFile("fuji.raf"));
    
    REQUIRE(features.detectedType == "image/x-fuji-raf");
    REQUIRE(features.rawFormat.has_value());
    REQUIRE(features.rawFormat->manufacturer == "Fujifilm");
}

TEST_CASE("IntelligentSelector: Recommend for JSON") {
    BenchmarkDatabase db;
    db.open("benchmark.db");
    
    IntelligentSelector selector;
    selector.loadBenchmarkDatabase("benchmark.db");
    
    auto rec = selector.analyze(loadFile("config.json"));
    
    REQUIRE(rec.preprocessor == "json-canon");
    REQUIRE(rec.algorithm == "brotli");
    REQUIRE(rec.level == 11);
    REQUIRE(rec.confidence > 0.8);
    REQUIRE(rec.expectedRatio < 0.3);  // 70%+ reduction
}

TEST_CASE("IntelligentSelector: Unknown file type uses heuristic") {
    IntelligentSelector selector;
    selector.loadBenchmarkDatabase("benchmark.db");
    
    auto rec = selector.analyze(loadFile("unknown.xyz"));
    
    REQUIRE(rec.confidence >= 0.0);
    REQUIRE(rec.confidence <= 0.6);  // Lower confidence for unknown
    REQUIRE_FALSE(rec.preprocessor.empty());
    REQUIRE_FALSE(rec.algorithm.empty());
}

TEST_CASE("IntelligentSelector: High entropy files get lower compression") {
    IntelligentSelector selector;
    auto features = selector.analyze(loadFile("random.bin"));
    
    REQUIRE(features.entropy > 7.5);
    
    auto rec = selector.analyze(loadFile("random.bin"));
    REQUIRE(rec.expectedRatio > 0.9);  // <10% compression
}

TEST_CASE("IntelligentSelector: Online learning updates database") {
    IntelligentSelector selector;
    selector.loadBenchmarkDatabase("benchmark.db");
    selector.setOnlineLearning(true, "user.db");
    
    // Record a result
    selector.recordResult("new_file.json", "json-canon", "brotli", 11, 
                         1024, 50ms);
    
    // Query should now include this result
    auto rec = selector.analyze(loadFile("new_file.json"));
    REQUIRE(rec.sampleCount > 0);
}
```

---

## Dependencies

### Required
- **SQLite 3** - Embedded database
- **OpenSSL** - SHA256 hashing

### Optional
- **libmagic** - Better MIME type detection
- **nlohmann/json** - JSON parsing for structure detection

### Build Integration

```cmake
# CMakeLists.txt
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(mosqueeze_intelligent
    src/FileAnalyzer.cpp
    src/BenchmarkDatabase.cpp
    src/IntelligentSelector.cpp
)

target_link_libraries(mosqueeze_intelligent
    PRIVATE
        SQLite::SQLite3
        OpenSSL::Crypto
)

# Optional dependencies
find_package(libmagic QUIET)
if(libmagic_FOUND)
    target_compile_definitions(mosqueeze_intelligent PRIVATE HAS_LIBMAGIC=1)
    target_link_libraries(mosqueeze_intelligent PRIVATE libmagic::libmagic)
endif()

find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    target_compile_definitions(mosqueeze_intelligent PRIVATE HAS_NLOHMANN_JSON=1)
    target_link_libraries(mosqueeze_intelligent PRIVATE nlohmann_json::nlohmann_json)
endif()
```

---

## Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| **Prediction accuracy** | >90% within 5% of actual ratio | Compare predicted vs actual on test corpus |
| **High confidence accuracy** | >95% for confidence > 0.9 | High confidence predictions should be accurate |
| **Low confidence fallback** | Graceful degradation | Unknown types still get reasonable recommendations |
| **Query time** | <100ms | Database lookup + ranking |
| **Database size** | <50MB | Precomputed benchmark results |
| **Coverage** | >50 file types | Supported out of the box |
| **Online learning improvement** | >5% accuracy gain after 100 files | Measured over time |

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Database size grows too large | Versioning, compression, cleanup of old entries |
| Slow queries | Indexes, caching, precomputed views |
| Unknown file types get poor recommendations | Heuristic fallback with low confidence |
| Online learning privacy concerns | Local-only, user-controlled, anonymize file hashes |
| Database becomes outdated | Version check, auto-update, bundle with releases |

---

## Future Enhancements

1. **ML Model** - Gradient boosting model for predictions
2. **Cloud sync** - Share anonymized benchmark data
3. **Custom algorithms** - User-defined compression plugins
4. **Regression testing** - Alert when prediction accuracy drops
5. **Per-user learning** - Personalized based on typical files
6. **Compression profiles** - "archive", "realtime", "backup" presets

---

## Acceptance Criteria

- [ ] `IntelligentSelector` class implemented
- [ ] `FileAnalyzer` class implemented with all detectors
- [ ] `BenchmarkDatabase` class implemented with SQLite
- [ ] Precomputed benchmark database included (~20-50MB)
- [ ] CLI commands: `mosqueeze-suggest`, `mosqueeze-analyze`, `mosqueeze-compress --interactive`
- [ ] Heuristic fallback for unknown file types
- [ ] Confidence scoring implemented
- [ ] Online learning optional feature
- [ ] Unit tests for all components (>90% coverage)
- [ ] Documentation: API, CLI, Architecture
- [ ] Performance: <100ms for recommendation
- [ ] Accuracy: >90% within 5% of actual ratio on test corpus