# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test Commands

```bash
# Configure (requires VCPKG_ROOT set)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build

# Run all tests
cd build && ctest --output-on-failure

# Run a single test by name pattern
cd build && ctest -R "ZstdEngine_test" --output-on-failure

# Run a single test directly
./build/tests/unit/ZstdEngine_test

# Build with gRPC server
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DMOSQUEEZE_BUILD_SERVER=ON
```

CMake options: `-DMOSQUEEZE_BUILD_TESTS=ON` (default), `-DMOSQUEEZE_BUILD_SERVER=OFF` (default).

## Architecture

MoSqueeze is a C++20 cross-platform cold-storage compression library with intelligent algorithm selection.

**Core library** (`src/libmosqueeze/`) — shared library with public headers in `include/mosqueeze/`:
- **Engines**: `ICompressionEngine` interface with implementations (ZstdEngine, LzmaEngine, BrotliEngine, ZpaqEngine). Each supports streaming compress/decompress with configurable levels.
- **Preprocessors**: `IPreprocessor` interface with `preprocess()`/`postprocess()` roundtrip. Implementations: JsonCanonicalizer, XmlCanonicalizer, ImageMetaStripper, BayerPreprocessor, PngOptimizer, DictionaryPreprocessor.
- **CompressionPipeline**: Orchestrates engine + preprocessor, writes trailing header (magic `0x4D535146`) after compressed payload for decompression roundtrip.
- **FileTypeDetector**: Detects file type via magic bytes, extension, and text sniffing; returns `FileClassification` (FileType enum, mimeType, isCompressed, canRecompress).
- **AlgorithmSelector**: Rule-based selection from `config/selection_rules.json`.
- **IntelligentSelector**: ML-like recommendation using BenchmarkDatabase (SQLite) and FileAnalyzer; returns `Recommendation` with alternatives and confidence scores.
- **PredictionEngine**: High-level prediction using embedded `DecisionMatrix` (JSON compiled into binary via `cmake/EmbedResources.cmake`).
- **FileAnalyzer**: Extracts `FileFeatures` (entropy, chunk ratio, structured detection).

**Executables**:
- `mosqueeze-cli` — CLI: `analyze`, `suggest`, `compress`, `decompress`, `predict` subcommands
- `mosqueeze-bench` — Benchmark runner with parallel execution, SQLite/JSON/CSV/HTML export
- `mosqueeze-viz` — Dashboard/report generator from benchmark SQLite data
- `mosqueeze-server` — gRPC service (optional, `-DMOSQUEEZE_BUILD_SERVER=ON`)

**Namespaces**: `mosqueeze` (library), `mosqueeze::cli` (CLI), `mosqueeze::server` (gRPC).

**Bundled dependency**: `third_party/zpaq/` is built as part of the project.

## Testing

Unit tests are standalone executables (each has its own `main()`) registered with CTest. The `tests/prediction/` directory uses Catch2. Test binaries link against `libmosqueeze`; on Windows, post-build commands copy the DLL next to executables.

Tests that use the decision matrix (`PredictionEngine_test`, `DecisionMatrix_test`) are compiled with `DECISION_MATRIX_JSON_PATH` pointing to `docs/benchmarks/combined_decision_matrix.json`.

## Key Patterns

- **Engine pattern**: New compression engines implement `ICompressionEngine` — must provide `name()`, `fileExtension()`, `supportedLevels()`, `compress()`, `decompress()`.
- **Preprocessor pattern**: New preprocessors implement `IPreprocessor` — must provide `preprocess()`/`postprocess()` for roundtrip.
- **Decision matrix embedding**: Benchmark data in `docs/benchmarks/combined_decision_matrix.json` is compiled into the library via CMake custom command generating `decision_matrix_data.hpp`.
- **Config-driven selection**: `config/selection_rules.json` maps FileType enums to algorithm recommendations.