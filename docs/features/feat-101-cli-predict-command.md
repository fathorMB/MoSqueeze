# Worker Spec: CLI predict Command

## Issue
#101 - CLI predict Command

## Objective
Add a `predict` subcommand to `mosqueeze-cli` that exposes the PredictionEngine via command line with JSON output support.

## Dependencies
- #99 - DecisionMatrix Provider
- #100 - PredictionEngine Core
- Existing CLI infrastructure in `src/mosqueeze-cli/`

## Files to Create/Modify

### 1. `src/mosqueeze-cli/src/PredictCommand.cpp` (NEW)
```cpp
#include <CLI/CLI.hpp>
#include <fmt/format.h>
#include <mosqueeze/PredictionEngine.hpp>
#include <mosqueeze/Version.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>

namespace {

void addPredictCommand(CLI::App& app) {
    auto* predict = app.add_subcommand("predict", "Predict compression options for a file");
    
    // Input options
    auto inputFile = std::make_shared<std::string>();
    auto outputFile = std::make_shared<std::string>("");
    auto format = std::make_shared<std::string>("text");
    auto preferSpeed = std::make_shared<bool>(false);
    auto decisionMatrixPath = std::make_shared<std::string>("");
    
    predict->add_option("file", *inputFile, "File to analyze")->required();
    predict->add_option("-o,--output", *outputFile, "Output file (default: stdout)");
    predict->add_option("-f,--format", *format, "Output format: text, json")
        ->check(CLI::IsMember({"text", "json"}));
    predict->add_flag("-s,--prefer-speed", *preferSpeed, "Prioritize fast algorithms");
    predict->add_option("--decision-matrix", *decisionMatrixPath, 
        "Path to decision matrix JSON (default: embedded)");
    
    predict->callback([inputFile, outputFile, format, preferSpeed, decisionMatrixPath]() {
        namespace fs = std::filesystem;
        
        fs::path path(*inputFile);
        if (!fs::exists(path)) {
            throw std::runtime_error("File does not exist: " + path.string());
        }
        
        // Configure prediction engine
        mosqueeze::PredictionConfig config;
        config.preferSpeed = *preferSpeed;
        if (!decisionMatrixPath->empty()) {
            config.decisionMatrixPath = *decisionMatrixPath;
        }
        
        mosqueeze::PredictionEngine engine;
        engine.setConfig(config);
        
        // Run prediction
        auto result = engine.predict(path);
        
        // Format output
        std::string output;
        if (*format == "json") {
            output = result.toJson(2);
        } else {
            output = formatText(result);
        }
        
        // Write output
        if (outputFile->empty()) {
            fmt::print("{}\n", output);
        } else {
            std::ofstream out(*outputFile);
            out << output << "\n";
            spdlog::info("Prediction written to {}", *outputFile);
        }
    });
}

std::string formatText(const mosqueeze::PredictionResult& result) {
    std::string out;
    
    out += fmt::format("File: {}\n", result.path.string());
    out += fmt::format("Size: {} bytes\n", result.inputSize);
    out += fmt::format("Type: {}\n", result.mimeType);
    
    if (result.shouldSkip) {
        out += fmt::format("\n⚠️  SKIP: {}\n", result.skipReason);
        return out;
    }
    
    if (!result.preprocessor.empty() && result.preprocessor != "none") {
        out += fmt::format("Preprocessor: {}\n", result.preprocessor);
    }
    
    out += "\n📊 Recommendations:\n";
    
    int i = 1;
    for (const auto& opt : result.recommendations) {
        std::string speedEmoji;
        if (opt.speedLabel == "fast") speedEmoji = "⚡";
        else if (opt.speedLabel == "medium") speedEmoji = "⏱️";
        else speedEmoji = "🐢";
        
        out += fmt::format("  {}. {} level {} {} {:.1f}x ratio\n",
            i++, opt.algorithm, opt.level, speedEmoji, opt.estimatedRatio);
        out += fmt::format("     Estimated size: {} bytes ({:.1f}% compression)\n",
            opt.estimatedSize, 
            100.0 * (1.0 - opt.estimatedSize / static_cast<double>(result.inputSize)));
        
        if (!opt.fallbackAlgorithm.empty()) {
            out += fmt::format("     Fallback: {} level {} ({:.1f}x)\n",
                opt.fallbackAlgorithm, opt.fallbackLevel, opt.fallbackRatio);
        }
    }
    
    return out;
}

} // namespace
```

### 2. `src/mosqueeze-cli/src/main.cpp` (MODIFY)
Add the predict subcommand registration:
```cpp
// Add after addAnalyzeCommand(app);

void addPredictCommand(CLI::App& app);  // Forward declaration

int main(int argc, char** argv) {
    CLI::App app{"MoSqueeze - Cold Storage Compression Library"};
    
    // ... existing flags ...
    
    addAnalyzeCommand(app);
    addPredictCommand(app);  // NEW
    
    CLI11_PARSE(app, argc, argv);
    // ...
}
```

### 3. `src/mosqueeze-cli/CMakeLists.txt` (MODIFY)
Add new source file:
```cmake
target_sources(mosqueeze-cli PRIVATE
    src/main.cpp
    src/PredictCommand.cpp  # NEW
)
```

## CLI Usage Examples

### Basic text output
```bash
$ mosqueeze predict document.json
File: document.json
Size: 1048576 bytes
Type: application/json
Preprocessor: json-canonical

📊 Recommendations:
  1. zpaq level 7 🐢 12.5x ratio
     Estimated size: 83886 bytes (92.0% compression)
  2. brotli level 11 ⏱️ 8.2x ratio
     Estimated size: 127997 bytes (87.8% compression)
  3. zstd level 3 ⚡ 5.4x ratio
     Estimated size: 194176 bytes (81.5% compression)
```

### JSON output (for programmatic consumption)
```bash
$ mosqueeze predict video.mp4 --format json
{
  "file": "video.mp4",
  "extension": ".mp4",
  "input_size": 52428800,
  "should_skip": true,
  "skip_reason": "Video (MP4) is already compressed with H.264/265 codec"
}
```

### Specify custom decision matrix
```bash
$ mosqueeze predict data.db --decision-matrix ./custom_matrix.json --format json
```

### Prefer fast algorithms
```bash
$ mosqueeze predict large_file.bin --prefer-speed
# Recommendations sorted by speed first
```

### Batch mode (multiple files)
```bash
$ mosqueeze predict file1.json file2.xml file3.db --format json --output predictions.json
```

## CLI Flag Reference

| Flag | Description |
|------|-------------|
| `file` | Input file path (required) |
| `-o, --output` | Output file (default: stdout) |
| `-f, --format` | Output format: `text` (default), `json` |
| `-s, --prefer-speed` | Prioritize fast algorithms |
| `--decision-matrix` | Custom decision matrix JSON path |
| `-v, --verbose` | Show debug information |

## Acceptance Criteria
- [x] `mosqueeze predict <file>` produces correct prediction
- [x] `--format json` outputs valid JSON
- [x] Text output is human-readable with formatting
- [x] Error handling for non-existent files
- [x] `--prefer-speed` changes recommendation order
- [x] Works with embedded decision matrix (no external file needed)

## Testing Strategy
1. Manual testing with all file types from decision matrix
2. Verify JSON output is parseable
3. Test skip files produce correct skip output
4. Compare predictions with actual compression results

## Estimated Effort
**Small** - 2-3 hours
- CLI flag parsing: 30 min
- Output formatting: 1 hour
- Testing: 1 hour

## Depends On
- #99 (DecisionMatrix Provider)
- #100 (PredictionEngine Core)

## Part of
- #98 (EPIC: Compression Prediction API)