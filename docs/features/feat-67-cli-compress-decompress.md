# Worker Spec: CLI Compress/Decompress Commands (#67)

## Issue
https://github.com/fathorMB/MoSqueeze/issues/67

## Overview
Add `compress` and `decompress` commands to `mosqueeze-cli` to enable programmatic compression from TheVault and other tools.

## Requirements

### `compress` command
```bash
mosqueeze compress <input> -o <output> -a <algorithm> [-l <level>] [--preprocess <type>] [--json]
```

| Flag | Description | Values |
|------|-------------|--------|
| `-a,--algorithm` | Compression algorithm | `zstd`, `brotli`, `lzma`, `zpaq` |
| `-l,--level` | Compression level | zstd: 1-22, brotli: 0-11, lzma: 0-9, zpaq: 1-5 |
| `-o,--output` | Output file path | Required |
| `--preprocess` | Preprocessor type | `none`, `json-canonical`, `xml-canonical`, `image-meta-strip`, `png-optimizer`, `bayer-raw` |
| `--json` | JSON output to stdout | Machine-readable format |
| `--level` defaults | zstd: 3, brotli: 6, lzma: 6, zpaq: 3 |

### `decompress` command
```bash
mosqueeze decompress <input> [-o <output>] [--json]
```

| Flag | Description |
|------|-------------|
| `-o,--output` | Output file (default: input without `.msz` extension) |
| `--json` | JSON output to stdout |

### JSON Output Format
```json
{
  "success": true,
  "input": "/path/to/input.dat",
  "output": "/path/to/output.msz",
  "inputSize": 1000000,
  "outputSize": 300000,
  "ratio": 0.30,
  "algorithm": "zstd",
  "level": 3,
  "preprocessor": "none"
}
```

Error format:
```json
{
  "success": false,
  "error": "Failed to compress: file not found",
  "input": "/path/to/input.dat"
}
```

### Stdin/Stdout Mode
When input is `-` or `-o -`:
- Read from stdin, write to stdout
- Useful for piping: `cat file.dat | mosqueeze compress - -a zstd | ...`

## Implementation

### New Files
```
src/mosqueeze-cli/src/commands/
├── CompressCommand.cpp
├── CompressCommand.hpp
├── DecompressCommand.cpp
└── DecompressCommand.hpp
```

### File Changes

#### `src/mosqueeze-cli/src/main.cpp`
Add subcommand registration:
```cpp
#include "commands/CompressCommand.hpp"
#include "commands/DecompressCommand.hpp"

// In main():
addCompressCommand(app);
addDecompressCommand(app);
```

#### `src/mosqueeze-cli/CMakeLists.txt`
Add new source files:
```cmake
target_sources(mosqueeze PRIVATE
    src/main.cpp
    src/commands/CompressCommand.cpp
    src/commands/DecompressCommand.cpp
)
```

### CompressCommand Implementation

```cpp
// src/mosqueeze-cli/src/commands/CompressCommand.hpp
#pragma once
#include <CLI/CLI.hpp>
#include <mosqueeze/CompressionPipeline.hpp>
#include <mosqueeze/engines/ZstdEngine.hpp>
#include <mosqueeze/engines/BrotliEngine.hpp>
#include <mosqueeze/engines/LzmaEngine.hpp>
#include <mosqueeze/engines/ZpaqEngine.hpp>

namespace mosqueeze::cli {

struct CompressOptions {
    std::string inputFile;
    std::string outputFile;
    std::string algorithm;
    int level{-1};  // -1 = use default
    std::string preprocess{"none"};
    bool jsonOutput{false};
};

void addCompressCommand(CLI::App& app);

ICompressionEngine* createEngine(const std::string& algorithm);

int runCompress(const CompressOptions& opts);

} // namespace mosqueeze::cli
```

```cpp
// src/mosqueeze-cli/src/commands/CompressCommand.cpp
#include "CompressCommand.hpp"
#include <mosqueeze/preprocessors/JsonCanonicalizer.hpp>
#include <mosqueeze/preprocessors/XmlCanonicalizer.hpp>
#include <mosqueeze/preprocessors/ImageMetaStripper.hpp>
#include <mosqueeze/preprocessors/PngOptimizer.hpp>
#include <mosquee/preprocessors/BayerPreprocessor.hpp>
#include <fmt/format.h>
#include <fstream>
#include <sstream>
#include <memory>

namespace mosqueeze::cli {

void addCompressCommand(CLI::App& app) {
    auto* compress = app.add_subcommand("compress", "Compress a file");
    
    auto opts = std::make_shared<CompressOptions>();
    
    compress->add_option("input", opts->inputFile, "Input file (use - for stdin)")
        ->required();
    compress->add_option("-o,--output", opts->outputFile, "Output file (use - for stdout)")
        ->required();
    compress->add_option("-a,--algorithm", opts->algorithm, "Algorithm: zstd, brotli, lzma, zpaq")
        ->default_val("zstd")
        ->check(CLI::IsMember({"zstd", "brotli", "lzma", "zpaq"}));
    compress->add_option("-l,--level", opts->level, "Compression level (default: algo-specific)");
    compress->add_option("--preprocess", opts->preprocess, "Preprocessor type")
        ->default_val("none")
        ->check(CLI::IsMember({"none", "json-canonical", "xml-canonical", "image-meta-strip", "png-optimizer", "bayer-raw"}));
    compress->add_flag("--json", opts->jsonOutput, "Output result as JSON");
    
    compress->callback([opts]() {
        const int exitCode = runCompress(*opts);
        if (exitCode != 0) {
            throw CLI::RuntimeError(exitCode);
        }
    });
}

ICompressionEngine* createEngine(const std::string& algorithm) {
    static ZstdEngine zstd;
    static BrotliEngine brotli;
    static LzmaEngine lzma;
    static ZpaqEngine zpaq;
    
    if (algorithm == "zstd") return &zstd;
    if (algorithm == "brotli") return &brotli;
    if (algorithm == "lzma") return &lzma;
    if (algorithm == "zpaq") return &zpaq;
    
    return &zstd; // default
}

int getDefaultLevel(const std::string& algorithm) {
    // Conservative defaults for cold storage
    if (algorithm == "zstd") return 3;
    if (algorithm == "brotli") return 6;
    if (algorithm == "lzma") return 6;
    if (algorithm == "zpaq") return 3;
    return 3;
}

int runCompress(const CompressOptions& opts) {
    try {
        // Validate input
        if (opts.inputFile != "-" && !std::filesystem::exists(opts.inputFile)) {
            if (opts.jsonOutput) {
                fmt::print("{{\"success\":false,\"error\":\"Input file not found\",\"input\":\"{}\"}}\n", opts.inputFile);
            } else {
                fmt::print(stderr, "Error: Input file not found: {}\n", opts.inputFile);
            }
            return 1;
        }
        
        // Determine level
        const int level = (opts.level >= 0) ? opts.level : getDefaultLevel(opts.algorithm);
        
        // Create engine and pipeline
        auto* engine = createEngine(opts.algorithm);
        CompressionPipeline pipeline(engine);
        
        // Set preprocessor if not "none"
        std::unique_ptr<IPreprocessor> preprocessor;
        if (opts.preprocess == "json-canonical") {
            preprocessor = std::make_unique<JsonCanonicalizer>();
        } else if (opts.preprocess == "xml-canonical") {
            preprocessor = std::make_unique<XmlCanonicalizer>();
        } else if (opts.preprocess == "image-meta-strip") {
            preprocessor = std::make_unique<ImageMetaStripper>();
        } else if (opts.preprocess == "png-optimizer") {
            preprocessor = std::make_unique<PngOptimizer>();
        } else if (opts.preprocess == "bayer-raw") {
            preprocessor = std::make_unique<BayerPreprocessor>();
        }
        
        if (preprocessor) {
            pipeline.setPreprocessor(preprocessor.get());
        }
        
        // Open streams
        std::ifstream inFile;
        std::ofstream outFile;
        std::istream* input = nullptr;
        std::ostream* output = nullptr;
        
        if (opts.inputFile == "-") {
            input = &std::cin;
        } else {
            inFile.open(opts.inputFile, std::ios::binary);
            input = &inFile;
        }
        
        if (opts.outputFile == "-") {
            output = &std::cout;
        } else {
            outFile.open(opts.outputFile, std::ios::binary);
            output = &outFile;
        }
        
        // Compress
        CompressionOptions compOpts;
        compOpts.level = level;
        
        const auto sizeBefore = opts.inputFile != "-" 
            ? std::filesystem::file_size(opts.inputFile) 
            : 0; // stdin size unknown
        
        auto result = pipeline.compress(*input, *output, compOpts);
        
        // Get output size
        const auto sizeAfter = opts.outputFile != "-"
            ? std::filesystem::file_size(opts.outputFile)
            : result.compression.outputSize; // from result
        
        // Output result
        if (opts.jsonOutput) {
            fmt::print(
                "{{\"success\":true,\"input\":\"{}\",\"output\":\"{}\","
                "\"inputSize\":{},\"outputSize\":{},\"ratio\":{:.3f},"
                "\"algorithm\":\"{}\",\"level\":{},\"preprocessor\":\"{}\"}}\n",
                opts.inputFile, opts.outputFile,
                sizeBefore, sizeAfter,
                static_cast<double>(sizeAfter) / static_cast<double>(sizeBefore ?: 1),
                opts.algorithm, level, opts.preprocess
            );
        } else {
            fmt::print("Compressed {} → {}\n", opts.inputFile, opts.outputFile);
            fmt::print("  Algorithm: {} level {}\n", opts.algorithm, level);
            fmt::print("  Size: {} → {} bytes ({:.1f}%)\n", 
                sizeBefore, sizeAfter, 
                100.0 * sizeAfter / (sizeBefore ?: 1));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        if (opts.jsonOutput) {
            fmt::print("{{\"success\":false,\"error\":\"{}\"}}\n", e.what());
        } else {
            fmt::print(stderr, "Error: {}\n", e.what());
        }
        return 2;
    }
}

} // namespace mosqueeze::cli
```

### DecompressCommand Implementation

```cpp
// src/mosqueeze-cli/src/commands/DecompressCommand.cpp
// Similar structure to CompressCommand but:
// - Detect algorithm from trailing header or file extension
// - Call pipeline.decompress()
// - Handle preprocessor reconstruction automatically
```

## Test Requirements

### Unit Tests (`tests/unit/CompressCommand_test.cpp`)
```cpp
// Test algorithm selection
TEST(CompressCommand, SelectsCorrectEngine) { ... }

// Test level defaults
TEST(CompressCommand, DefaultLevelsPerAlgorithm) { ... }

// Test JSON output format
TEST(CompressCommand, JsonOutputFormat) { ... }

// Test error handling
TEST(CompressCommand, FileNotFoundError) { ... }
```

### Integration Tests
```bash
# Basic compression
./mosqueeze compress test.dat -o test.msz -a zstd

# Roundtrip
./mosqueeze compress test.dat -o test.msz -a zstd
./mosqueeze decompress test.msz -o test.restored
diff test.dat test.restored  # should be identical

# JSON output parsing
./mosqueeze compress test.dat -o test.msz -a zstd --json | jq .success  # should be true
```

## Edge Cases

### Input Does Not Exist
- Exit code: 1
- JSON: `{"success": false, "error": "Input file not found"}`

### Invalid Algorithm
- CLI11 will reject before callback

### Invalid Level for Algorithm
- Engine will throw; catch and report

### Stdin Compression (input size unknown)
- JSON: `inputSize: 0`, `ratio: 0.0` (can't calculate)

## Dependencies

### Must Use
- `CompressionPipeline` from libmosqueeze (already exists)
- All engine implementations (already exist)
- All preprocessor implementations (already exist)

### No New Dependencies
- Uses existing CLI11, fmt, spdlog

## Exit Codes
| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Input file not found |
| 2 | Compression/decompression error |
| 3 | Invalid arguments (handled by CLI11) |

## Performance Considerations
- Files are read entirely into memory (via istream)
- For large files (>1GB), consider streaming implementation later
- Preprocessors may need extra memory for in-memory transformation

## Future Enhancements (Out of Scope)
- Progress reporting during compression
- Multi-file batch compression
- Resume partial compression
- Custom output extension (currently `.msz`)