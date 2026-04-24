# Worker Spec: DecisionMatrix Provider

## Issue
#99 - DecisionMatrix Provider

## Objective
Implement a `DecisionMatrix` class that loads benchmark data and provides fast lookups for compression recommendations based on file extension.

## Dependencies
- `docs/benchmarks/combined_decision_matrix.json` (existing)
- `src/libmosqueeze/include/mosqueeze/bench/BenchmarkResult.hpp` (existing)

## Files to Create/Modify

### 1. `src/libmosqueeze/include/mosqueeze/DecisionMatrix.hpp`
```cpp
#pragma once

#include <mosqueeze/bench/BenchmarkResult.hpp>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace mosqueeze {

// Speed classification for user-facing recommendations
enum class CompressionSpeed {
    Fast,      // < 100ms
    Medium,    // 100ms - 1s  
    Slow       // > 1s
};

// Single recommendation with all metadata
struct PredictionOption {
    std::string algorithm;     // "zstd", "brotli", "lzma", "zpaq"
    int level;                 // Compression level
    double estimatedRatio;     // Expected compression ratio
    size_t estimatedSize;     // Predicted compressed size (when input size known)
    CompressionSpeed speed;    // User-facing speed indicator
    std::string speedLabel;   // "fast", "medium", "slow"
    
    // Optional fallback (second-best option)
    std::string fallbackAlgorithm;
    int fallbackLevel;
    double fallbackRatio;
};

// Complete prediction result for a file
struct Prediction {
    bool shouldSkip;                           // true for already-compressed files
    std::string skipReason;                    // rationale if skipping
    std::string fileExtension;                 // ".json", ".mp4", etc.
    FileType fileType;                         // Classified type
    std::vector<PredictionOption> options;    // Ordered by recommendation strength
    
    // Quick access helpers
    const PredictionOption* getBest() const;
    const PredictionOption* getFastest() const;
    const PredictionOption* getBalanced() const; // best ratio/speed tradeoff
};

class DecisionMatrix {
public:
    DecisionMatrix();
    
    // Load from embedded resource or external file
    bool loadDefault();
    bool loadFromFile(const std::filesystem::path& path);
    
    // Query methods
    Prediction predict(const std::string& extension, size_t fileSize = 0) const;
    Prediction predict(const std::filesystem::path& file) const;
    
    // Check if extension has benchmark data
    bool hasData(const std::string& extension) const;
    
    // Get all known extensions
    std::vector<std::string> knownExtensions() const;
    
    // Statistics
    size_t totalEntries() const;
    size_t totalBenchmarks() const;
    
private:
    struct ExtensionData {
        bool skip = false;
        std::string skipReason;
        size_t fileCount = 0;
        size_t testCount = 0;
        double avgRatio = 1.0;
        
        // Best ratio result
        std::string bestRatioAlgo;
        int bestRatioLevel = 0;
        double bestRatio = 1.0;
        
        // Best fast (< 100ms) result
        std::string bestFastAlgo;
        int bestFastLevel = 0;
        double bestFastRatio = 1.0;
        
        // Best balanced result  
        std::string bestBalancedAlgo;
        int bestBalancedLevel = 0;
        double bestBalancedRatio = 1.0;
        
        // Raw results for flexibility
        std::vector<bench::BenchmarkResult> results;
    };
    
    std::unordered_map<std::string, ExtensionData> data_;
    std::string generatedDate_;
    size_t totalBenchmarks_ = 0;
    
    CompressionSpeed classifySpeed(std::chrono::milliseconds encodeTime) const;
    std::string fileExtensionToLower(const std::filesystem::path& path) const;
};

} // namespace mosqueeze
```

### 2. `src/libmosqueeze/src/DecisionMatrix.cpp`
Implement all methods:
- `loadDefault()`: Load from embedded JSON resource (compile-time embedding)
- `loadFromFile()`: Parse JSON decision matrix at runtime
- `predict()`: Return structured prediction with speed classification
- `hasData()`: Check extension exists

### 3. `src/libmosqueeze/cmake/EmbedResources.cmake` (NEW)
CMake module to embed `combined_decision_matrix.json` as compiled-in resource:
```cmake
# Generate header with embedded decision matrix
add_custom_command(
    OUTPUT decision_matrix_data.hpp
    COMMAND ${CMAKE_COMMAND} -DINPUT=${DECISION_MATRIX_PATH}
            -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/decision_matrix_data.hpp
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedData.cmake
    DEPENDS ${DECISION_MATRIX_PATH}
)
```

## Implementation Notes

### Decision Matrix JSON Schema (existing)
```json
{
  "generated": "2026-04-24T...",
  "decision_matrix": {
    ".json": {
      "file_count": 15,
      "test_count": 45,
      "avg_ratio": 8.23,
      "best_ratio": 12.5,
      "best_ratio_algo": "zpaq/7",
      "best_fast": {...},
      "best_balanced": {...}
    }
  }
}
```

### Speed Classification Logic
```cpp
CompressionSpeed DecisionMatrix::classifySpeed(std::chrono::milliseconds t) const {
    if (t.count() < 100) return CompressionSpeed::Fast;
    if (t.count() < 1000) return CompressionSpeed::Medium;
    return CompressionSpeed::Slow;
}
```

### Prediction Priority Order
1. **Best Overall** - Highest ratio (may be slow)
2. **Fastest** - Best ratio under 100ms
3. **Balanced** - Best ratio/time tradeoff (user recommendation default)

## Acceptance Criteria
- [x] DecisionMatrix loads benchmark data from embedded resource
- [x] `predict(extension)` returns valid Prediction for all known extensions
- [x] `predict(file)` works with full path (extracts extension)
- [x] Unknown extensions return Prediction with fallback to zstd/22
- [x] Speed labels correct: "fast", "medium", "slow"
- [x] Skip files (.jpg, .mp4, .7z, etc.) return shouldSkip=true with reason

## Testing Strategy
1. Unit tests for each public method
2. Test with all 34 extensions in decision matrix
3. Test fallback for unknown extensions
4. Test skip files return correct skip reason

## Estimated Effort
**Medium** - 4-6 hours
- Header definition: 30 min
- JSON parsing: 2 hours
- Resource embedding: 1.5 hours  
- Testing: 2 hours

## Follows After
This is the foundation. No dependencies on other new code.

## Blocks
- #100: PredictionEngine (needs DecisionMatrix)
- #101: CLI predict command (needs DecisionMatrix)