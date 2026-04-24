# Worker Spec: PredictionEngine Core

## Issue
#100 - PredictionEngine Core

## Objective
Implement `PredictionEngine` that combines file type detection, preprocessor selection, and decision matrix to produce complete compression predictions.

## Dependencies
- #99 - DecisionMatrix Provider (must be completed first)
- `src/libmosqueeze/include/mosquette/FileTypeDetector.hpp` (existing)
- `src/libmosqueeze/include/mosqueeze/PreprocessorSelector.hpp` (existing)
- `src/libmosqueeze/include/mosqueeze/AlgorithmSelector.hpp` (existing)

## Files to Create/Modify

### 1. `src/libmosqueeze/include/mosqueeze/PredictionEngine.hpp`
```cpp
#pragma once

#include <mosqueeze/DecisionMatrix.hpp>
#include <mosqueeze/FileTypeDetector.hpp>
#include <mosqueeze/PreprocessorSelector.hpp>
#include <filesystem>
#include <string>
#include <memory>
#include <functional>

namespace mosqueeze {

// Callback for progress reporting during prediction
using ProgressCallback = std::function<void(const std::string& stage, double progress)>;

// Configuration for prediction behavior
struct PredictionConfig {
    bool enablePreprocessing = true;      // Consider preprocessor gains
    bool preferSpeed = false;             // Prioritize fast over best ratio
    bool includeFallbacks = true;         // Include fallback options
    size_t fileSizeThreshold = 100 * 1024 * 1024; // 100MB - warn for slow algos
    std::filesystem::path decisionMatrixPath; // Empty = use embedded
};

// Result of prediction - ready for JSON serialization
struct PredictionResult {
    // File information
    std::filesystem::path path;
    std::string extension;
    size_t inputSize = 0;
    size_t estimatedOutputSize = 0;
    
    // Skip handling
    bool shouldSkip = false;
    std::string skipReason;
    
    // Classification
    std::string mimeType;
    FileType fileType = FileType::Unknown;
    
    // Preprocessor recommendation
    std::string preprocessor;        // "none", "json-canonical", etc.
    bool preprocessorAvailable = false;
    
    // Recommendations (ordered by suitability)
    std::vector<PredictionOption> recommendations;
    
    // Convenience accessors
    const PredictionOption* primary() const;
    const PredictionOption* fastest() const;
    const PredictionOption* balanced() const;  // Best ratio/time tradeoff
    
    // Serialization
    std::string toJson(int indent = 2) const;
    static PredictionResult fromJson(const std::string& json);
};

class PredictionEngine {
public:
    PredictionEngine();
    ~PredictionEngine();
    
    // Configuration
    void setConfig(const PredictionConfig& config);
    const PredictionConfig& config() const;
    
    // Primary prediction interface
    PredictionResult predict(const std::filesystem::path& file,
                             ProgressCallback callback = nullptr) const;
    
    // Batch prediction for multiple files
    std::vector<PredictionResult> predictBatch(
        const std::vector<std::filesystem::path>& files,
        ProgressCallback callback = nullptr) const;
    
    // Check prediction capabilities
    bool hasDataFor(const std::string& extension) const;
    std::vector<std::string> supportedExtensions() const;
    
    // Statistics about underlying data
    struct Stats {
        size_t totalBenchmarks = 0;
        size_t supportedExtensions = 0;
        std::string dataGenerated;
        std::vector<std::string> skipExtensions;
    };
    Stats getStats() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    // Internal helpers
    std::string determinePreprocessor(const FileClassification& classification) const;
    bool isLargeFile(size_t size) const;
};

} // namespace mosqueeze
```

### 2. `src/libmosqueeze/src/PredictionEngine.cpp`

Implement `PredictionEngine`:

```cpp
#include <mosqueeze/PredictionEngine.hpp>
#include <mosqueeze/DecisionMatrix.hpp>
#include <mosqueeze/FileTypeDetector.hpp>
#include <mosqueeze/PreprocessorSelector.hpp>
#include <mosqueeze/AlgorithmSelector.hpp>
#include <nlohmann/json.hpp>
#include <fstream>

namespace mosqueeze {

class PredictionEngine::Impl {
public:
    PredictionConfig config;
    DecisionMatrix decisionMatrix;
    FileTypeDetector typeDetector;
    PreprocessorSelector preprocessorSelector;
    
    bool initialize() {
        if (!config.decisionMatrixPath.empty()) {
            return decisionMatrix.loadFromFile(config.decisionMatrixPath);
        }
        return decisionMatrix.loadDefault();
    }
};

PredictionEngine::PredictionEngine() 
    : impl_(std::make_unique<Impl>()) {
    impl_->initialize();
}

PredictionResult PredictionEngine::predict(const std::filesystem::path& file,
                                           ProgressCallback callback) const {
    PredictionResult result;
    result.path = file;
    
    // Get file size
    if (std::filesystem::exists(file)) {
        result.inputSize = std::filesystem::file_size(file);
    }
    
    // Extract extension
    result.extension = file.extension().string();
    std::transform(result.extension.begin(), result.extension.end(),
                   result.extension.begin(), ::tolower);
    
    // Classify file type
    if (callback) callback("classifying", 0.1);
    auto classification = impl_->typeDetector.detect(file);
    result.mimeType = classification.mimeType;
    result.fileType = classification.type;
    
    // Check if should skip
    if (classification.recommendation == "skip" || !classification.canRecompress) {
        if (callback) callback("skip", 1.0);
        result.shouldSkip = true;
        result.skipReason = classification.mimeType + " is already compressed or not suitable";
        return result;
    }
    
    // Determine preprocessor
    if (impl_->config.enablePreprocessing) {
        if (callback) callback("preprocessing", 0.3);
        result.preprocessor = determinePreprocessor(classification);
        result.preprocessorAvailable = !result.preprocessor.empty() && 
                                       result.preprocessor != "none";
    }
    
    // Get recommendations from decision matrix
    if (callback) callback("predicting", 0.5);
    auto prediction = impl_->decisionMatrix.predict(result.extension, result.inputSize);
    
    // Convert to result
    result.shouldSkip = prediction.shouldSkip;
    result.skipReason = prediction.skipReason;
    result.recommendations = std::move(prediction.options);
    
    // Calculate estimated output sizes
    for (auto& opt : result.recommendations) {
        opt.estimatedSize = static_cast<size_t>(result.inputSize / opt.estimatedRatio);
    }
    
    // Sort by preference (balanced first if configured)
    if (impl_->config.preferSpeed) {
        std::sort(result.recommendations.begin(), result.recommendations.end(),
            [](const auto& a, const auto& b) {
                if (a.speed != b.speed) return a.speed < b.speed;
                return a.estimatedRatio > b.estimatedRatio;
            });
    }
    
    if (callback) callback("complete", 1.0);
    return result;
}

std::string PredictionResult::toJson(int indent) const {
    nlohmann::json j;
    j["file"] = path.string();
    j["extension"] = extension;
    j["input_size"] = inputSize;
    j["estimated_output_size"] = estimatedOutputSize;
    j["should_skip"] = shouldSkip;
    if (shouldSkip) {
        j["skip_reason"] = skipReason;
    }
    j["mime_type"] = mimeType;
    j["preprocessor"] = preprocessor;
    
    j["recommendations"] = nlohmann::json::array();
    for (const auto& opt : recommendations) {
        nlohmann::json opt_j;
        opt_j["algorithm"] = opt.algorithm;
        opt_j["level"] = opt.level;
        opt_j["estimated_ratio"] = opt.estimatedRatio;
        opt_j["estimated_size"] = opt.estimatedSize;
        opt_j["speed"] = opt.speedLabel;
        
        if (!opt.fallbackAlgorithm.empty()) {
            opt_j["fallback_algorithm"] = opt.fallbackAlgorithm;
            opt_j["fallback_level"] = opt.fallbackLevel;
            opt_j["fallback_ratio"] = opt.fallbackRatio;
        }
        
        j["recommendations"].push_back(opt_j);
    }
    
    return j.dump(indent);
}

// ... rest of implementation

} // namespace mosqueeze
```

### 3. Update `src/libmosqueeze/CMakeLists.txt`

Add new source files to build:
```cmake
target_sources(mosqueeze PRIVATE
    src/DecisionMatrix.cpp
    src/PredictionEngine.cpp
)
```

## Implementation Notes

### Preprocessor Detection Logic
```cpp
std::string PredictionEngine::Impl::determinePreprocessor(const FileClassification& c) const {
    // JSON canonicalization for JSON files
    if (c.mimeType == "application/json" || c.extension == ".json") {
        return "json-canonical";
    }
    // XML canonicalization for XML files
    if (c.mimeType == "application/xml" || c.extension == ".xml") {
        return "xml-canonical";
    }
    // Bayer preprocessing for RAW images
    if (c.type == FileType::Image_Raw) {
        return "bayer-raw";
    }
    return "none";
}
```

### JSON Output Format
```json
{
  "file": "/path/to/document.json",
  "extension": ".json",
  "input_size": 1048576,
  "estimated_output_size": 131072,
  "should_skip": false,
  "mime_type": "application/json",
  "preprocessor": "json-canonical",
  "recommendations": [
    {
      "algorithm": "zpaq",
      "level": 7,
      "estimated_ratio": 8.0,
      "estimated_size": 131072,
      "speed": "slow"
    },
    {
      "algorithm": "brotli",
      "level": 11,
      "estimated_ratio": 7.2,
      "estimated_size": 145635,
      "speed": "medium"
    },
    {
      "algorithm": "zstd",
      "level": 3,
      "estimated_ratio": 5.4,
      "estimated_size": 194176,
      "speed": "fast"
    }
  ]
}
```

## Acceptance Criteria
- [x] `PredictionEngine::predict(file)` returns complete prediction
- [x] `PredictionResult::toJson()` produces valid JSON
- [x] Correct preprocessor detection for JSON, XML, RAW files
- [x] Skip detection for already-compressed files (JPEG, MP4, etc.)
- [x] Recommendations ordered by estimated ratio
- [x] Fallback recommendations included when available
- [x] Progress callback invoked at each stage

## Testing Strategy
1. Unit tests with known file types
2. Test skip detection for all skip extensions
3. Test preprocessor selection accuracy
4. Test JSON serialization roundtrip

## Estimated Effort
**Medium** - 3-4 hours
- Header definition: 30 min
- Implementation: 2 hours
- Testing: 1.5 hours

## Depends On
- #99 (DecisionMatrix Provider)

## Blocks
- #101 (CLI predict command)
- #102 (Tests)